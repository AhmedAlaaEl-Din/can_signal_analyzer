.pragma library

// Integer with thin grouping: 1234567 -> "1,234,567"
function grouped(n) {
    const v = Math.round(Number(n) || 0)
    const s = Math.abs(v).toString()
    let out = ""
    for (let i = 0; i < s.length; ++i) {
        if (i > 0 && (s.length - i) % 3 === 0)
            out += ","
        out += s[i]
    }
    return (v < 0 ? "-" : "") + out
}

// Fraction 0..1 -> "44.2 %"
function percent(f, digits) {
    return (Number(f) * 100).toFixed(digits === undefined ? 1 : digits) + " %"
}

// Microseconds -> "0:12.345" (minutes:seconds.milliseconds), hours if needed.
function clock(us) {
    const total = Math.max(0, Math.round(Number(us) / 1000))   // ms
    const ms = total % 1000
    const s = Math.floor(total / 1000) % 60
    const m = Math.floor(total / 60000) % 60
    const h = Math.floor(total / 3600000)
    const pad = (v, n) => v.toString().padStart(n, "0")
    return (h > 0 ? h + ":" + pad(m, 2) : m.toString()) + ":" + pad(s, 2) + "." + pad(ms, 3)
}

// Microseconds -> "1:02" (compact, for durations of recordings).
function duration(us) {
    const s = Math.floor(Math.max(0, Number(us)) / 1e6)
    const m = Math.floor(s / 60)
    return m + ":" + (s % 60).toString().padStart(2, "0")
}

// Bytes -> "1.4 MB" / "820 KB"
function bytes(n) {
    const v = Number(n) || 0
    if (v >= 1048576) return (v / 1048576).toFixed(v >= 104857600 ? 0 : 1) + " MB"
    if (v >= 1024) return Math.round(v / 1024) + " KB"
    return v + " B"
}
