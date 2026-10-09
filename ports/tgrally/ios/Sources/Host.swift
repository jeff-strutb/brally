// Host.swift: the iPhone host -- what the platform layers ask of the
// operating system (ports/brally/platform/host/host.h) that host/posix does
// not already give: the window (a Metal layer the view controller owns), the
// event queues the games' threads drain, the pad (touch, TouchPad.swift),
// Core Audio out, the save folders and Boss Rally's disc.
//
// Two games share it (Engines.swift): Top Gear Rally, and Boss Rally when it
// runs the races. One is in front at a time: the events, the sound and the
// screen are its; the other waits.
//
// Each function is exported under its C name; the Swift names differ so they
// do not collide with host.h's declarations imported through Bridge.h.

import AVFoundation
import AudioToolbox
import QuartzCore
import UIKit

// ---- process -------------------------------------------------------------------

/// Each game's save folder, in Application Support.
private let saveDirs: [UnsafeMutablePointer<CChar>] = ["Top Gear Rally", "Boss Rally"].map { name in
    let base = FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask).first!
    let dir = base.appendingPathComponent(name, isDirectory: true)
    try? FileManager.default.createDirectory(at: dir, withIntermediateDirectories: true)
    return strdup(dir.path)
}
private let discDir: UnsafeMutablePointer<CChar> = strdup((Bundle.main.resourcePath ?? ".") + "/disc")

@_cdecl("host_set_app_name")
func hostSetAppName(_ dir: UnsafePointer<CChar>?, _ title: UnsafePointer<CChar>?) {}

@_cdecl("host_init")
func hostInit(_ argc: Int32, _ argv: UnsafeMutablePointer<UnsafeMutablePointer<CChar>?>?) {}

@_cdecl("host_save_dir")
func hostSaveDir() -> UnsafePointer<CChar>? { UnsafePointer(saveDirs[Engines.caller().rawValue]) }

/// Boss Rally's CD root: its data track, in the app
@_cdecl("host_cd_dir")
func hostCdDir() -> UnsafePointer<CChar>? { UnsafePointer(discDir) }

/// The CD's audio tracks are not in the app: Boss Rally's music is off.
@_cdecl("host_music_dir")
func hostMusicDir() -> UnsafePointer<CChar>? { nil }

@_cdecl("host_stream_open")
func hostStreamOpen(_ path: UnsafePointer<CChar>?, _ rate: Int32) -> OpaquePointer? { nil }

@_cdecl("host_stream_read")
func hostStreamRead(_ s: OpaquePointer?, _ lr: UnsafeMutablePointer<Float>?, _ frames: Int32) -> Int32 { 0 }

@_cdecl("host_stream_close")
func hostStreamClose(_ s: OpaquePointer?) {}

/// Only Top Gear Rally ends the app's sound (Boss Rally stays behind it).
@_cdecl("host_shutdown")
func hostShutdown() { if Engines.caller() == .tgr { hostAudioClose() } }

@_cdecl("host_message_box")
func hostMessageBox(_ text: UnsafePointer<CChar>?, _ caption: UnsafePointer<CChar>?) {
    NSLog("%@: %@", caption.map { String(cString: $0) } ?? "", text.map { String(cString: $0) } ?? "")
}

// ---- the window: the view controller's Metal layer -------------------------------

/// The layer the renderer draws into, and its size in pixels: set on the main
/// thread when the view lays out, read on the game's.
enum Screen {
    static var layer: CAMetalLayer?
    private static let lock = NSLock()
    private static var w: Int32 = 0, h: Int32 = 0

    static func setPixels(_ pw: Int32, _ ph: Int32) {
        lock.lock(); w = pw; h = ph; lock.unlock()
    }
    static func pixels() -> (Int32, Int32) {
        lock.lock(); defer { lock.unlock() }
        return (w, h)
    }
}

@_cdecl("host_window_open")
func hostWindowOpen(_ w: Int32, _ h: Int32, _ title: UnsafePointer<CChar>?) -> Int32 {
    Screen.layer != nil ? 1 : 0
}

/// The game in front sees its window; the other draws nothing meanwhile.
@_cdecl("host_window_visible")
func hostWindowVisible() -> Int32 { Engines.active == Engines.caller() ? 1 : 0 }

@_cdecl("host_window_lock_aspect")
func hostWindowLockAspect(_ lock: Int32) {}      // the screen's shape is fixed (16:9, landscape)

@_cdecl("host_present")
func hostPresent(_ argb: UnsafePointer<UInt32>?, _ w: Int32, _ h: Int32) {}   // Metal presents itself

@_cdecl("host_macos_metal_layer")
func hostMetalLayer() -> UnsafeMutableRawPointer? {
    Screen.layer.map { Unmanaged.passUnretained($0).toOpaque() }
}

@_cdecl("host_macos_layer_fit")
func hostLayerFit(_ w: UnsafeMutablePointer<Int32>, _ h: UnsafeMutablePointer<Int32>) {
    (w.pointee, h.pointee) = Screen.pixels()
}

@_cdecl("host_window_pixels")
func hostWindowPixels(_ w: UnsafeMutablePointer<Int32>, _ h: UnsafeMutablePointer<Int32>) {
    (w.pointee, h.pointee) = Screen.pixels()
}

// ---- events: the app pushes, the game's main loop drains --------------------------

/// The game in front gets the events; each drains its own queue.
enum Events {
    private static let cond = NSCondition()
    private static var queues: [[host_event]] = [[], []]

    static func push(_ e: host_event) {
        cond.lock(); queues[Engines.active.rawValue].append(e); cond.broadcast(); cond.unlock()
    }
    static func focus(_ on: Bool) {
        var e = host_event()
        e.type = Int32(HOST_EV_FOCUS)
        e.down = on ? 1 : 0
        push(e)
    }
    static func pop(_ ev: UnsafeMutablePointer<host_event>, _ waitMs: UInt32) -> Int32 {
        let q = Engines.caller().rawValue
        cond.lock(); defer { cond.unlock() }
        if queues[q].isEmpty && waitMs > 0 {
            let until = waitMs == 0xFFFF_FFFF ? Date.distantFuture
                                              : Date(timeIntervalSinceNow: Double(waitMs) / 1000)
            while queues[q].isEmpty && cond.wait(until: until) {}
        }
        if queues[q].isEmpty { return 0 }
        ev.pointee = queues[q].removeFirst()
        return 1
    }
}

@_cdecl("host_poll_event")
func hostPollEvent(_ ev: UnsafeMutablePointer<host_event>, _ waitMs: UInt32) -> Int32 {
    Events.pop(ev, waitMs)
}

// ---- the pad: touch (TouchPad.swift) ------------------------------------------------

@_cdecl("host_pad_read")
func hostPadRead(_ p: UnsafeMutablePointer<host_pad>) -> Int32 {
    var o = host_pad()
    o.pov = -1
    let s = TouchPad.shared.sample()
    o.x = s.x
    o.y = s.y
    o.buttons = s.buttons
    p.pointee = o
    return 1
}

// ---- audio out: a RemoteIO unit pulling the games' mixers -------------------------------
// Each game opens its sound once (its mixer and rate are kept). The unit runs at
// the first one's rate (Top Gear Rally's). While Boss Rally races, the music is
// still Top Gear Rally's (its mixer plays on: src/racing/handoff.c) and Boss
// Rally's effects are added, taken from its own rate to the unit's.

private var audioUnit: AudioComponentInstance?
private var audioFns: [host_audio_fn?] = [nil, nil]
private var audioUsers: [UnsafeMutableRawPointer?] = [nil, nil]
private var audioRates: [Int32] = [0, 0]
private var audioRate: Int32 = 0


private let audioRender: AURenderCallback = { _, _, _, _, frames, io in
    guard let io else { return noErr }
    let buf = UnsafeMutableAudioBufferListPointer(io)[0]
    guard let data = buf.mData else { return noErr }
    let out = data.assumingMemoryBound(to: Float.self)
    let n = Int(frames)
    memset(data, 0, Int(buf.mDataByteSize))
    for e in 0..<2 where audioFns[e] != nil {
        if e == Engine.br.rawValue && Engines.active != .br { continue }   // between races it is silent
        if audioRates[e] == audioRate {
            if e == 0 {
                audioFns[e]!(out, Int32(n), audioUsers[e])
            } else {
                audioFns[e]!(audioScratch, Int32(n), audioUsers[e])
                for i in 0..<2 * n { out[i] += audioScratch[i] }
            }
        } else {
            addResampled(e, out, n)
        }
    }
    return noErr
}

/// Room for a callback's frames from a mixer (stereo), and the other rate's
/// resampling state: the position between its samples, its last sample
private let audioScratchFrames = 8192
private let audioScratch = UnsafeMutablePointer<Float>.allocate(capacity: 2 * (8192 + 2))
private var resamplePos = 0.0
private var resampleLast: (Float, Float) = (0, 0)

/// A mixer at another rate than the unit's, added in: its samples read
/// between with a straight line (its effects only, close to the unit's rate)
private func addResampled(_ e: Int, _ out: UnsafeMutablePointer<Float>, _ n: Int) {
    guard let fn = audioFns[e], audioRates[e] > 0, audioRate > 0 else { return }
    let ratio = Double(audioRates[e]) / Double(audioRate)
    // src[0] is the last sample of the call before; src[1...] new ones
    let need = Int(floor(resamplePos + Double(n - 1) * ratio)) + 1
    guard need + 1 <= audioScratchFrames else { return }
    audioScratch[0] = resampleLast.0
    audioScratch[1] = resampleLast.1
    fn(audioScratch + 2, Int32(need), audioUsers[e])
    for i in 0..<n {
        let x = resamplePos + Double(i) * ratio
        let k = Int(x)
        let f = Float(x - Double(k))
        out[2 * i] += audioScratch[2 * k] * (1 - f) + audioScratch[2 * k + 2] * f
        out[2 * i + 1] += audioScratch[2 * k + 1] * (1 - f) + audioScratch[2 * k + 3] * f
    }
    let end = resamplePos + Double(n) * ratio
    let used = Int(end)
    resampleLast = (audioScratch[2 * used], audioScratch[2 * used + 1])
    resamplePos = end - Double(used)
}

@_cdecl("host_audio_open")
func hostAudioOpen(_ rate: Int32, _ fn: host_audio_fn?, _ user: UnsafeMutableRawPointer?) -> Int32 {
    let e = Engines.caller().rawValue
    audioFns[e] = fn
    audioUsers[e] = user
    audioRates[e] = rate
    if audioUnit != nil {
        return 1
    }
    let session = AVAudioSession.sharedInstance()
    try? session.setCategory(.playback, mode: .default, options: [])
    try? session.setPreferredSampleRate(Double(rate))
    try? session.setPreferredIOBufferDuration(0.010)
    try? session.setActive(true)

    var desc = AudioComponentDescription(componentType: kAudioUnitType_Output,
                                         componentSubType: kAudioUnitSubType_RemoteIO,
                                         componentManufacturer: kAudioUnitManufacturer_Apple,
                                         componentFlags: 0, componentFlagsMask: 0)
    guard let comp = AudioComponentFindNext(nil, &desc) else { return 0 }
    var unit: AudioComponentInstance?
    guard AudioComponentInstanceNew(comp, &unit) == noErr, let unit else { return 0 }
    var fmt = AudioStreamBasicDescription(mSampleRate: Float64(rate), mFormatID: kAudioFormatLinearPCM,
                                          mFormatFlags: kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked,
                                          mBytesPerPacket: 8, mFramesPerPacket: 1, mBytesPerFrame: 8,
                                          mChannelsPerFrame: 2, mBitsPerChannel: 32, mReserved: 0)
    var cb = AURenderCallbackStruct(inputProc: audioRender, inputProcRefCon: nil)
    audioRate = rate
    if AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0,
                            &fmt, UInt32(MemoryLayout.size(ofValue: fmt))) != noErr ||
        AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0,
                             &cb, UInt32(MemoryLayout.size(ofValue: cb))) != noErr ||
        AudioUnitInitialize(unit) != noErr || AudioOutputUnitStart(unit) != noErr {
        AudioComponentInstanceDispose(unit)
        return 0
    }
    audioUnit = unit
    return 1
}

func hostAudioClose() {
    guard let unit = audioUnit else { return }
    AudioOutputUnitStop(unit)
    AudioUnitUninitialize(unit)
    AudioComponentInstanceDispose(unit)
    audioUnit = nil
}
