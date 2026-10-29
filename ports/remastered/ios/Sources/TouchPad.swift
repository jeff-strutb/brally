// TouchPad.swift: the N64 pad from fingers on the screen.
//
// In a race (tgr_view_driving, or br_race_driving while Boss Rally runs it):
//   the wheel  a finger on the right of the brake zone holds the accelerator
//              (A) for as long as it stays on the glass; moving it sideways
//              steers, full lock at 90 points either side, the neutral point
//              sliding along when the finger goes past the lock so the thumb
//              never runs out of room. A wheel drawn where the finger landed
//              turns with the steering. Lifting the finger coasts.
//   the brake  a finger on the left of the screen brakes and, once the car
//              has stopped, reverses, for as long as it is held; the
//              accelerator is let go meanwhile (the pedal wins). Boss Rally
//              does this itself: its reverse (the gamepad's button 2) drives
//              backwards at any speed, slowing the car first. Top Gear Rally
//              reverses only from a standstill in first gear (A with the
//              stick pulled back), so in its own races the pedal is the brake
//              (B) until the car is below a walk, then that.
//
// Elsewhere (menus, the pause menu) a tap or a swipe goes to the game's
// targets (platform/os/touch.c): tap an item, a row, an arrow or a button
// prompt; swipe to move, the content following the finger. Boss Rally's
// pause menu: a tap on a row chooses it, a choice on the Remaster Options
// page sets it (br_race_tap); a swipe is an arrow key (the same way round).
//
// In the paint shop (tgr_pointer_wanted) a finger is the pointer
// (platform/os/touch.c, src/menus/rmpaintptr.c): the cursor goes where it
// is and it holds A, so a finger paints, drags a line or a shape out, or taps
// a tool, a colour or a popup's choice; a tap with two fingers is B where
// the first landed (on the decal, the colour there; elsewhere, cancel). A
// finger is held back a moment (paintHold) before it presses, so a second
// one landing makes the two a two-finger tap rather than a dot.
//
// While a race is driven, the pause button at the top centre (two bars)
// pauses it. Anywhere, a tap with two fingers is START (pause), and a tap
// with three steps the sound's low-pass filter (AudioFilter), its setting
// shown a moment.

import UIKit

/// Which game is in front (ports/remastered/platform/mux.c): the one the
/// touches drive. Boss Rally while it runs a race, else Top Gear Rally.
enum Front {
    static var bossRally: Bool { Int(mux_front_game()) == MUX_BR }
}

final class TouchPad {
    static let shared = TouchPad()

    static let steerRange: CGFloat = 90     // points of sideways travel to full lock
    static let brakeZone: CGFloat = 0.3     // the left of the screen that brakes
    static let wheelSize: CGFloat = 160
    static let wheelDegrees: CGFloat = 120  // the wheel's turn at full lock

    private let lock = NSLock()
    private var steer: Float = 0
    private var gas = false
    private var brake = false
    private var reversing = false           // the pedal has turned to reverse (Top Gear Rally)

    static let reverseBelowKph: Float = 3

    /// What the game reads: host_pad's stick (-1..1, y positive downward) and buttons.
    func sample() -> (x: Float, y: Float, buttons: UInt32) {
        lock.lock(); defer { lock.unlock() }
        let x = gas ? steer : 0
        guard brake else {
            reversing = false
            return (x, 0, gas ? 1 << 0 : 0)
        }
        if Front.bossRally {
            return (x, 0, 1 << 2)                                   // its reverse: brakes, then backs up
        }
        if !reversing && rm_tgr_race_speed() < TouchPad.reverseBelowKph { reversing = true }
        return reversing ? (x, 1, 1 << 0) : (x, 0, 1 << 1)          // A, the stick pulled back : B
    }

    func drive(_ on: Bool, steer s: Float) {
        lock.lock(); gas = on; steer = on ? s : 0; lock.unlock()
    }

    func setBrake(_ on: Bool) {
        lock.lock(); brake = on; if !on { reversing = false }; lock.unlock()
    }

    func release() {
        lock.lock(); gas = false; brake = false; reversing = false; steer = 0; lock.unlock()
    }
}

/// The full-screen surface that turns touches into the pad and the menus' taps.
final class TouchSurface: UIView {
    static let tapMaxSeconds = 0.25
    static let tapMaxPoints: CGFloat = 12
    static let swipeMinPoints: CGFloat = 30

    private enum Role { case wheel, brake, menu, paint, none }

    /// A finger in the paint shop presses once it has been down this long, or moved
    static let paintHold = 0.06
    private struct Finger { var role: Role; var start: CGPoint; var began: Double }
    private var fingers: [UITouch: Finger] = [:]
    private var wheelOrigin = CGPoint.zero

    // one gesture: from the first finger down to the last up
    private var gestureBegan = 0.0
    private var gestureFingers = 0
    private var gestureMoved = false

    // the finger that is the paint shop's pointer: pressed (A) once it is held or moves
    private var paintTouch: UITouch?
    private var paintPressed = false
    private var paintTwo = false            // a second finger came before the press: a two-finger tap
    private var lastPaintStart = CGPoint.zero
    /// The area the game is drawn in, for a tap's place in it.
    var gameFrame = CGRect.zero

    private let wheel = WheelView(frame: CGRect(x: 0, y: 0, width: TouchPad.wheelSize, height: TouchPad.wheelSize))
    private let pedal = PedalView(frame: CGRect(x: 0, y: 0, width: 84, height: 128))
    private let note = UILabel()
    private let pause = PauseButton(frame: CGRect(x: 0, y: 0, width: 56, height: 44))
    private var link: CADisplayLink?

    /// The game in front at the last screen refresh: when the other comes
    /// in front, the fingers on the glass are let go
    private var lastFront = false

    override init(frame: CGRect) {
        super.init(frame: frame)
        isMultipleTouchEnabled = true
        backgroundColor = .clear
        wheel.isHidden = true
        pedal.isHidden = true
        addSubview(pedal)
        addSubview(wheel)
        note.font = .systemFont(ofSize: 15, weight: .semibold)
        note.textColor = .white
        note.backgroundColor = UIColor(white: 0, alpha: 0.6)
        note.layer.cornerRadius = 8
        note.layer.masksToBounds = true
        note.alpha = 0
        addSubview(note)
        pause.isHidden = true
        pause.addTarget(self, action: #selector(pausePressed), for: .touchUpInside)
        addSubview(pause)
        link = CADisplayLink(target: self, selector: #selector(tick))
        link?.add(to: .main, forMode: .common)
    }

    required init?(coder: NSCoder) { fatalError() }

    override func layoutSubviews() {
        super.layoutSubviews()
        let zone = bounds.width * TouchPad.brakeZone
        pedal.center = CGPoint(x: (zone * 0.5).rounded(), y: (bounds.height * 0.62).rounded())
        let top = max(safeAreaInsets.top, 6)
        pause.center = CGPoint(x: (gameFrame.width > 0 ? gameFrame.midX : bounds.midX).rounded(), y: (top + 24).rounded())
    }

    /// The pause button: the race's own pause, as Escape is on a keyboard.
    @objc private func pausePressed() {
        if Front.bossRally {
            br_race_key(Int32(BR_KEY_ESCAPE))
        } else {
            rm_tgr_touch_press(0x1000)                                     // START
        }
    }

    /// The pedal is shown while a race is driven; a hand-over starts afresh.
    @objc private func tick() {
        if let t = paintTouch, !paintPressed, !paintTwo, let f = fingers[t],
           CACurrentMediaTime() - f.began >= TouchSurface.paintHold {
            paintPress(f.start)
        }
        if Front.bossRally != lastFront {
            lastFront = Front.bossRally
            reset()
        }
        let driving = TouchSurface.driving
        if pedal.isHidden == driving { pedal.isHidden = !driving }
        if pause.isHidden == driving { pause.isHidden = !driving; setNeedsLayout() }
    }

    private var hasWheel: Bool { fingers.values.contains { $0.role == .wheel } }
    private var hasBrake: Bool { fingers.values.contains { $0.role == .brake } }

    private func role(for p: CGPoint) -> Role {
        if TouchSurface.driving {
            if p.x < bounds.width * TouchPad.brakeZone { return .brake }
            return hasWheel ? .none : .wheel
        }
        if !Front.bossRally && rm_tgr_pointer_wanted() != 0 {
            return paintTouch == nil ? .paint : .none
        }
        return fingers.values.contains { $0.role == .menu } ? .none : .menu
    }

    /// A place on the screen as fractions of the area the game is drawn in
    private func fraction(_ p: CGPoint) -> (Float, Float) {
        guard gameFrame.width > 0, gameFrame.height > 0 else { return (0, 0) }
        return (Float((p.x - gameFrame.minX) / gameFrame.width), Float((p.y - gameFrame.minY) / gameFrame.height))
    }

    private func pointer(_ p: CGPoint, _ buttons: Int32) {
        let (u, v) = fraction(p)
        rm_tgr_pointer(u, v, buttons)
    }

    /// The paint finger presses where it landed
    private func paintPress(_ start: CGPoint) {
        paintPressed = true
        pointer(start, 1)
    }

    private func beginWheel(at p: CGPoint) {
        wheelOrigin = p
        TouchPad.shared.drive(true, steer: 0)
        wheel.center = p
        wheel.transform = .identity
        wheel.isHidden = false
    }

    override func touchesBegan(_ touches: Set<UITouch>, with event: UIEvent?) {
        let now = CACurrentMediaTime()
        for t in touches {
            if fingers.isEmpty {
                gestureBegan = now
                gestureFingers = 0
                gestureMoved = false
                paintTwo = false
            }
            gestureFingers += 1
            let p = t.location(in: self)
            let r = role(for: p)
            fingers[t] = Finger(role: r, start: p, began: now)
            switch r {
            case .wheel: beginWheel(at: p)
            case .brake: TouchPad.shared.setBrake(true); pedal.pressed = true
            case .paint: paintTouch = t; paintPressed = false; paintTwo = false; lastPaintStart = p
            case .none: if paintTouch != nil && !paintPressed { paintTwo = true }
            default: break
            }
        }
    }

    override func touchesMoved(_ touches: Set<UITouch>, with event: UIEvent?) {
        for t in touches {
            guard let f = fingers[t] else { continue }
            let p = t.location(in: self)
            if hypot(p.x - f.start.x, p.y - f.start.y) > TouchSurface.tapMaxPoints { gestureMoved = true }
            if f.role == .paint {
                if !paintPressed && !paintTwo && hypot(p.x - f.start.x, p.y - f.start.y) > TouchSurface.tapMaxPoints / 2 {
                    paintPress(f.start)
                }
                if paintPressed { pointer(p, 1) }
                continue
            }
            guard f.role == .wheel else { continue }
            var raw = p.x - wheelOrigin.x
            if raw > TouchPad.steerRange {
                wheelOrigin.x = p.x - TouchPad.steerRange; raw = TouchPad.steerRange
            } else if raw < -TouchPad.steerRange {
                wheelOrigin.x = p.x + TouchPad.steerRange; raw = -TouchPad.steerRange
            }
            let s = raw / TouchPad.steerRange
            TouchPad.shared.drive(true, steer: Float(s))
            wheel.center = CGPoint(x: wheelOrigin.x, y: wheel.center.y)   // the wheel follows the sliding neutral
            wheel.transform = CGAffineTransform(rotationAngle: s * TouchPad.wheelDegrees * .pi / 180)
        }
    }

    override func touchesEnded(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches, cancelled: false) }
    override func touchesCancelled(_ touches: Set<UITouch>, with event: UIEvent?) { end(touches, cancelled: true) }

    private func end(_ touches: Set<UITouch>, cancelled: Bool) {
        let now = CACurrentMediaTime()
        for t in touches {
            guard let f = fingers.removeValue(forKey: t) else { continue }
            let p = t.location(in: self)
            switch f.role {
            case .wheel:
                TouchPad.shared.drive(false, steer: 0)
                wheel.isHidden = true
            case .brake:
                if !hasBrake { TouchPad.shared.setBrake(false); pedal.pressed = false }
            case .paint:
                paintTouch = nil
                if paintPressed {
                    pointer(p, 0)
                } else if !cancelled && !paintTwo {
                    pointer(f.start, 1)                                     // a quick tap: a click there
                    pointer(f.start, 0)
                }
                paintPressed = false
            case .menu:
                guard !cancelled, gestureFingers == 1 else { break }
                let dx = p.x - f.start.x, dy = p.y - f.start.y
                let dist = hypot(dx, dy)
                if dist <= TouchSurface.tapMaxPoints && now - f.began <= TouchSurface.tapMaxSeconds {
                    tap(at: p)
                } else if dist >= TouchSurface.swipeMinPoints {
                    swipe(dx, dy)
                }
            case .none:
                break
            }
        }
        if fingers.isEmpty && !cancelled && !gestureMoved && now - gestureBegan <= TouchSurface.tapMaxSeconds + 0.15 {
            if gestureFingers == 2 && paintTwo {
                pointer(lastPaintStart, 2)                                  // B where the first finger landed
                pointer(lastPaintStart, 0)
            } else if gestureFingers == 2 {
                if Front.bossRally {
                    br_race_key(Int32(BR_KEY_ESCAPE))                       // its pause
                } else {
                    rm_tgr_touch_press(0x1000)                                 // START
                }
            } else if gestureFingers == 3 {
                show(AudioFilter.step())
            }
        }
    }

    /// A line of text at the top of the screen for a moment.
    private func show(_ text: String) {
        note.text = "  \(text)  "
        note.sizeToFit()
        note.center = CGPoint(x: bounds.midX, y: safeAreaInsets.top + 34)
        note.alpha = 1
        UIView.animate(withDuration: 0.4, delay: 1.6, options: [], animations: { self.note.alpha = 0 })
    }

    /// The game in front's race is being driven: touch is the wheel and the pedals.
    static var driving: Bool {
        Front.bossRally ? br_race_driving() != 0 : rm_tgr_view_driving() != 0
    }

    private func swipe(_ dx: CGFloat, _ dy: CGFloat) {
        guard Front.bossRally else {
            rm_tgr_touch_swipe(Float(dx), Float(dy))
            return
        }
        let key = abs(dx) >= abs(dy) ? (dx < 0 ? BR_KEY_RIGHT : BR_KEY_LEFT) : (dy < 0 ? BR_KEY_DOWN : BR_KEY_UP)
        br_race_key(Int32(key))
    }

    private func tap(at p: CGPoint) {
        guard gameFrame.width > 0, gameFrame.height > 0 else { return }
        let nx = Float((p.x - gameFrame.minX) / gameFrame.width), ny = Float((p.y - gameFrame.minY) / gameFrame.height)
        if Front.bossRally {
            br_race_tap(nx, ny)
            return
        }
        rm_tgr_touch_tap(nx, ny)
    }

    func reset() {
        if paintPressed { pointer(lastPaintStart, 0) }
        paintTouch = nil
        paintPressed = false
        paintTwo = false
        fingers.removeAll()
        wheel.isHidden = true
        pedal.pressed = false
        TouchPad.shared.release()
    }
}

/// The sound's low-pass filter: the game's mix carries a fizz above 8 kHz that
/// the console's own output and a television softened (platform/audio/out.c).
/// The cut is kept between runs.
enum AudioFilter {
    static let settings = [0, 8000, 7000, 6000, 5000]       // Hz; 0: off
    static let key = "audioLowpassHz"

    static var current: Int {
        UserDefaults.standard.object(forKey: key) as? Int ?? 5000
    }

    static func apply() { rm_tgr_audio_lowpass(Int32(current)) }

    /// The next setting; its name.
    static func step() -> String {
        let i = settings.firstIndex(of: current) ?? 0
        let next = settings[(i + 1) % settings.count]
        UserDefaults.standard.set(next, forKey: key)
        apply()
        return next == 0 ? "Sound filter: off" : "Sound filter: \(next / 1000) kHz"
    }
}

/// The steering wheel drawn under the finger: a rim, a hub and three spokes.
final class WheelView: UIView {
    override init(frame: CGRect) {
        super.init(frame: frame)
        isUserInteractionEnabled = false
        backgroundColor = .clear
        alpha = 0.9
        let k = frame.width / 100, gold = UIColor(red: 0xf5 / 255.0, green: 0xd3 / 255.0, blue: 0x4a / 255.0, alpha: 1)
        let rim = CAShapeLayer()
        rim.path = UIBezierPath(ovalIn: CGRect(x: 6 * k, y: 6 * k, width: 88 * k, height: 88 * k)).cgPath
        rim.fillColor = nil
        rim.strokeColor = gold.cgColor
        rim.lineWidth = 9 * k
        layer.addSublayer(rim)
        let hub = CAShapeLayer()
        hub.path = UIBezierPath(ovalIn: CGRect(x: 39 * k, y: 39 * k, width: 22 * k, height: 22 * k)).cgPath
        hub.fillColor = gold.cgColor
        layer.addSublayer(hub)
        let spokes = UIBezierPath()
        for (a, b) in [((50, 40), (50, 10)), ((59, 56), (84, 70)), ((41, 56), (16, 70))] {
            spokes.move(to: CGPoint(x: CGFloat(a.0) * k, y: CGFloat(a.1) * k))
            spokes.addLine(to: CGPoint(x: CGFloat(b.0) * k, y: CGFloat(b.1) * k))
        }
        let sp = CAShapeLayer()
        sp.path = spokes.cgPath
        sp.strokeColor = gold.cgColor
        sp.lineWidth = 7 * k
        sp.lineCap = .round
        layer.addSublayer(sp)
        layer.shadowColor = gold.cgColor
        layer.shadowOpacity = 0.45
        layer.shadowRadius = 8
        layer.shadowOffset = .zero
    }

    required init?(coder: NSCoder) { fatalError() }
}

/// The brake pedal on the left: a ribbed pad, brighter while pressed.
final class PedalView: UIView {
    var pressed = false {
        didSet {
            guard pressed != oldValue else { return }
            alpha = pressed ? 0.85 : 0.4
            transform = pressed ? CGAffineTransform(scaleX: 0.94, y: 0.94) : .identity
        }
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        isUserInteractionEnabled = false
        backgroundColor = .clear
        alpha = 0.4
        let red = UIColor(red: 0xe8 / 255.0, green: 0x4a / 255.0, blue: 0x3c / 255.0, alpha: 1)
        let pad = CAShapeLayer()
        pad.path = UIBezierPath(roundedRect: bounds.insetBy(dx: 4, dy: 4), cornerRadius: 16).cgPath
        pad.fillColor = UIColor(white: 0.08, alpha: 0.55).cgColor
        pad.strokeColor = red.cgColor
        pad.lineWidth = 4
        layer.addSublayer(pad)
        let ribs = UIBezierPath()
        for i in 0..<4 {
            let y = 22 + CGFloat(i) * 16
            ribs.move(to: CGPoint(x: 22, y: y))
            ribs.addLine(to: CGPoint(x: bounds.width - 22, y: y))
        }
        let rb = CAShapeLayer()
        rb.path = ribs.cgPath
        rb.strokeColor = red.cgColor
        rb.lineWidth = 5
        rb.lineCap = .round
        layer.addSublayer(rb)
        let label = UILabel(frame: CGRect(x: 0, y: bounds.height - 36, width: bounds.width, height: 22))
        label.text = "BRAKE"
        label.textAlignment = .center
        label.font = .systemFont(ofSize: 13, weight: .heavy)
        label.textColor = red
        addSubview(label)
    }

    required init?(coder: NSCoder) { fatalError() }
}

/// The pause button at the top centre while a race is driven: two bars.
final class PauseButton: UIControl {
    override init(frame: CGRect) {
        super.init(frame: frame)
        backgroundColor = .clear
        let bars = CAShapeLayer()
        let p = UIBezierPath()
        for x in [21.0, 35.0] as [CGFloat] {
            p.append(UIBezierPath(roundedRect: CGRect(x: x - 4, y: 10, width: 8, height: 24), cornerRadius: 2))
        }
        bars.path = p.cgPath
        bars.fillColor = UIColor.white.cgColor
        bars.shadowColor = UIColor.black.cgColor
        bars.shadowOpacity = 0.6
        bars.shadowRadius = 3
        bars.shadowOffset = .zero
        layer.addSublayer(bars)
        alpha = 0.75
    }

    required init?(coder: NSCoder) { fatalError() }

    override var isHighlighted: Bool { didSet { alpha = isHighlighted ? 1 : 0.75 } }
}
