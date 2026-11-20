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
