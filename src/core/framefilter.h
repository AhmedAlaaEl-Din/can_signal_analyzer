#pragma once

#include "canframe.h"

#include <QString>

#include <cstdint>
#include <vector>

namespace can {

// Value filter for frames. Each criterion is optional; an empty filter accepts
// everything. Syntax (case-insensitive, separators: comma, semicolon or space):
//   ID:  "123", "0x1A4", "100-1FF", "0A0, 123, 600-6FF"   (hex)
//   DLC: "8", "0-4", "2, 8"                              (decimal 0..8)
class FrameFilter
{
public:
    enum class Format { Any = 0, Standard = 1, Extended = 2 };

    struct Range
    {
        std::uint32_t lo;
        std::uint32_t hi;
        bool operator==(const Range &) const = default;
    };

    // Parse helpers. On failure they return false and write a short,
    // user-friendly message into *error (if given); the output is untouched.
    static bool parseIdList(const QString &text, std::vector<Range> &out, QString *error = nullptr);
    static bool parseDlcList(const QString &text, std::vector<Range> &out, QString *error = nullptr);

    // Setters return false (and keep the previous criterion) on invalid input.
    bool setIdExpression(const QString &text, QString *error = nullptr);
    bool setDlcExpression(const QString &text, QString *error = nullptr);
    void setFormat(Format format) { m_format = format; }

    const std::vector<Range> &idRanges() const { return m_ids; }
    const std::vector<Range> &dlcRanges() const { return m_dlcs; }
    Format format() const { return m_format; }

    bool isActive() const { return !m_ids.empty() || !m_dlcs.empty() || m_format != Format::Any; }
    bool matches(const CanFrame &frame) const;

    bool operator==(const FrameFilter &) const = default;

private:
    std::vector<Range> m_ids;
    std::vector<Range> m_dlcs;
    Format m_format = Format::Any;
};

} // namespace can
