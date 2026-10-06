"""Neural Amp Modeler WaveNet inference (the .nam v0.5 format), following NeuralAmpModelerCore's
weight order and processing: per layer array a 1x1 rechannel, then dilated causal convolutions with
the input mixed back in, activations summed into a head accumulator, residual 1x1s, and a head
rechannel feeding the next array; output = head_scale * last head."""
import json, torch, torch.nn.functional as F

class NAM:
    def __init__(self, path):
        cfg = json.load(open(path)); assert cfg['architecture'] == 'WaveNet', cfg['architecture']
        c = cfg['config']; w = torch.tensor(cfg['weights'], dtype=torch.float32); self.i = 0; self.w = w
        take = lambda n: self._take(n)
        self.arrays = []
        for L in c['layers']:
            A = {'L': L}
            A['rechannel'] = take(L['channels'] * L['input_size']).view(L['channels'], L['input_size'], 1)
            mult = 2 if L['gated'] else 1
            A['layers'] = []
            for d in L['dilations']:
                conv_w = take(mult * L['channels'] * L['channels'] * L['kernel_size']).view(mult * L['channels'], L['channels'], L['kernel_size'])
                conv_b = take(mult * L['channels'])
                mix_w = take(mult * L['channels'] * L['condition_size']).view(mult * L['channels'], L['condition_size'], 1)
                one_w = take(L['channels'] * L['channels']).view(L['channels'], L['channels'], 1)
                one_b = take(L['channels'])
                A['layers'].append((d, conv_w, conv_b, mix_w, one_w, one_b))
            A['head_w'] = take(L['head_size'] * L['channels']).view(L['head_size'], L['channels'], 1)
            A['head_b'] = take(L['head_size']) if L['head_bias'] else None
            self.arrays.append(A)
        self.head_scale = float(take(1)[0])
        assert self.i == len(w), (self.i, len(w))
        self.act = {'Tanh': torch.tanh, 'ReLU': torch.relu, 'Sigmoid': torch.sigmoid}
        self.rf = sum((a['L']['kernel_size'] - 1) * d for a in self.arrays for d in a['L']['dilations']) + 1

    def _take(self, n):
        # weights are stored row-major per output channel, input channel, then kernel tap
        v = self.w[self.i:self.i + n]; self.i += n; return v

    @torch.no_grad()
    def __call__(self, x):
        x = torch.as_tensor(x, dtype=torch.float32).view(1, 1, -1)
        pad = self.rf - 1
        xp = F.pad(x, (pad, 0))                          # causal history of zeros
        cond = xp; inp = xp; head = None
        for A in self.arrays:
            L = A['L']; act = self.act[L['activation']]
            h = F.conv1d(inp, A['rechannel'])
            acc = head if head is not None else torch.zeros(1, L['channels'], h.shape[-1])
            for d, cw, cb, mw, ow, ob in A['layers']:
                z = F.conv1d(F.pad(h, ((L['kernel_size'] - 1) * d, 0)), cw, cb, dilation=d) + F.conv1d(cond, mw)
                if L['gated']:
                    C = L['channels']; z = torch.tanh(z[:, :C]) * torch.sigmoid(z[:, C:])
                else:
                    z = act(z)
                acc = acc + z
                h = h + F.conv1d(z, ow, ob)
            head = F.conv1d(acc, A['head_w'], A['head_b'])
            inp = h
        return (self.head_scale * head).view(-1)[pad:].numpy()
