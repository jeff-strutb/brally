"""Open Splice INSTRUMENT's own window so a patch can be chosen by hand; the plugin's state is saved every
two seconds while the window is open (and once more when it closes)."""
import pedalboard, re, sys, threading, time, splice_state as S
out = sys.argv[1]
p = pedalboard.load_plugin(sys.argv[2] if len(sys.argv) > 2 else '/Library/Audio/Plug-Ins/VST3/Splice INSTRUMENT.vst3')
done = threading.Event()
def saver():
    last = None
    while not done.is_set():
        time.sleep(2)
        try:
            st = p.raw_state; open(out, 'wb').write(st)            # write first: never lose a pick
            i_ = st.find(b'<META'); meta = st[i_:i_ + 120].decode('latin1') if i_ >= 0 else '?'
            if meta != last: print('state:', meta[:140], flush=True); last = meta
        except Exception as e: print('save error', e, flush=True)
threading.Thread(target=saver, daemon=True).start()
p.show_editor(done)
done.set(); open(out, 'wb').write(p.raw_state); print('closed; saved', out, flush=True)
