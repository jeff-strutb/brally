import pedalboard, juce_state as J, numpy as np, mido, sys, re
PLUG = '/Library/Audio/Plug-Ins/VST3/Splice INSTRUMENT.vst3'
def with_patch(p, family, name, artic=None):
    x, m = J.split(p.raw_state); d = J.jdecode(m.group(1).strip()).decode('latin1')
    d = re.sub(r'<META family="[^"]*" name="[^"]*"', '<META family="%s" name="%s"' % (family, name), d)
    if artic: d = d.replace('<SETTING id="a_name" value="Strings Pad"/>', '<SETTING id="a_name" value="%s"/>' % artic)
    # the inner blob carries its own length fields; keep the byte layout, only the XML text changed length
    return d
def render(p, secs=3.0, note=60):
    msgs = [mido.Message('note_on', note=note, velocity=100, time=0.1), mido.Message('note_off', note=note, velocity=0, time=2.0)]
    return p(msgs, duration=secs, sample_rate=48000, num_channels=2)
if __name__ == '__main__':
    p = pedalboard.load_plugin(PLUG); y = render(p); print('default', y.shape, float(np.abs(y).max()))
