/*
 * Copyright (c) 2024, Barcelona Supercomputing Center
 * Contact: mess             [at] bsc [dot] es
 *          pouya.esmaili [at] bsc [dot] es
 *          petar.radojkovic [at] bsc [dot] es
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *     * Neither the name of the copyright holder nor the names
 *       of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "mess_mem_ctrl.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <utility>

// Custom round function to avoid dependency issues
static double roundDouble(double d) {
    return std::floor(d + 0.5);
}

namespace {

/**
 * @brief A single read-percentage curve as parsed from the JSON file.
 */
struct CurveEntry {
    int readPercentage;                              ///< Read percentage in [0, 100].
    std::vector<std::pair<double, double>> points;   ///< Bandwidth (MB/s), latency (ns) pairs.
};

/**
 * @brief In-memory representation of a curve JSON file.
 *
 * Expected schema:
 * @code
 *   { "measuredChannels": <int>,
 *     "curves": { "<read_pct>": [[bw, lat], ...], ... } }
 * @endcode
 *
 * Curves are stored in the order they appear in the file. The number of
 * curves is determined dynamically by the file contents.
 */
struct CurveFile {
    uint32_t measuredChannels = 0;   ///< Channels the curves were measured on.
    std::vector<CurveEntry> curves;  ///< All parsed curves, in file order.
};

/**
 * @brief Throws a ``std::runtime_error`` describing a JSON parse failure.
 *
 * Marked ``[[noreturn]]`` so the compiler can elide fall-through paths in
 * the parser.
 *
 * @param msg Short message describing the parse error.
 */
[[noreturn]] static void parseError(const char* msg) {
    throw std::runtime_error(std::string("JSON parse error: ") + msg);
}

/**
 * @brief Advances ``p`` past any ASCII whitespace.
 *
 * Avoids the locale-aware ``std::isspace`` overhead by inlining the four
 * whitespace characters that JSON allows.
 *
 * @param p   Cursor into the JSON buffer; updated in place.
 * @param end One-past-the-end of the JSON buffer.
 */
static inline void skipWs(const char*& p, const char* end) {
    while (p < end) {
        char c = *p;
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            ++p;
        } else {
            break;
        }
    }
}

/**
 * @brief Skips whitespace and consumes the expected character or throws.
 *
 * @param p   Cursor into the JSON buffer; advanced past the consumed char.
 * @param end One-past-the-end of the JSON buffer.
 * @param c   Character expected at the current position.
 */
static inline void expectChar(const char*& p, const char* end, char c) {
    skipWs(p, end);
    if (p >= end || *p != c) parseError("unexpected character");
    ++p;
}

/**
 * @brief Skips whitespace and reports whether ``c`` is at the cursor.
 *
 * Does not consume the character.
 *
 * @param p   Cursor into the JSON buffer; advanced past whitespace.
 * @param end One-past-the-end of the JSON buffer.
 * @param c   Character to look for at the current position.
 * @return ``true`` if ``*p == c``, ``false`` otherwise.
 */
static inline bool peekChar(const char*& p, const char* end, char c) {
    skipWs(p, end);
    return p < end && *p == c;
}

/**
 * @brief Reads a JSON string and returns its raw extent inside the buffer.
 *
 * Assumes the schema contains no escape sequences (keys are either ASCII
 * field names or decimal integers, never quoted with backslashes), so no
 * copy is made.
 *
 * @param p      Cursor into the JSON buffer; advanced past the closing quote.
 * @param end    One-past-the-end of the JSON buffer.
 * @param outBeg Pointer to the first character inside the quotes.
 * @param outLen Length of the string in bytes.
 */
static inline void readRawString(const char*& p, const char* end,
                                 const char*& outBeg, size_t& outLen) {
    expectChar(p, end, '"');
    outBeg = p;
    while (p < end && *p != '"') ++p;
    if (p >= end) parseError("unterminated string");
    outLen = static_cast<size_t>(p - outBeg);
    ++p; // consume closing quote
}

/**
 * @brief Reads a JSON number using ``strtod`` directly on the buffer.
 *
 * The caller guarantees a trailing ``'\0'`` so ``strtod`` always has a
 * valid terminator. No allocation is performed.
 *
 * @param p   Cursor into the JSON buffer; advanced past the number.
 * @param end One-past-the-end of the JSON buffer.
 * @return The parsed double value.
 */
static inline double readNumber(const char*& p, const char* end) {
    skipWs(p, end);
    if (p >= end) parseError("expected number");
    char* endptr = nullptr;
    double v = std::strtod(p, &endptr);
    if (endptr == p) parseError("expected number");
    p = endptr;
    return v;
}

/**
 * @brief Reads a quoted decimal integer (used for curve keys like "42").
 *
 * @param p   Cursor into the JSON buffer; advanced past the closing quote.
 * @param end One-past-the-end of the JSON buffer.
 * @return The integer value parsed from inside the quotes.
 */
static inline int readQuotedInt(const char*& p, const char* end) {
    const char* beg;
    size_t len;
    readRawString(p, end, beg, len);
    if (len == 0) parseError("empty integer key");
    char* endptr = nullptr;
    long v = std::strtol(beg, &endptr, 10);
    if (endptr != beg + len) parseError("invalid integer key");
    return static_cast<int>(v);
}

/**
 * @brief Loads and parses a curve JSON file into a ``CurveFile``.
 *
 * The file is slurped in a single ``read()`` into a NUL-terminated buffer,
 * then scanned with raw ``const char*`` pointers. Numbers are parsed with
 * ``strtod``/``strtol`` directly on the buffer; key dispatch uses
 * ``memcmp`` against the two known top-level field names.
 *
 * @param path Filesystem path to the JSON file.
 * @return A populated ``CurveFile`` with ``measuredChannels`` and the
 *         per-read-percentage bandwidth-latency curves.
 * @throws std::runtime_error If the file cannot be read or parsed, or if
 *         ``measuredChannels`` is missing or zero.
 */
static CurveFile loadCurveFile(const std::string& path) {
    // Slurp the file in a single read; reserve exact capacity.
    std::ifstream fh(path, std::ios::binary | std::ios::ate);
    if (!fh.is_open()) {
        throw std::runtime_error("Failed to open curve file: " + path);
    }
    std::streamsize size = fh.tellg();
    if (size < 0) {
        throw std::runtime_error("Failed to size curve file: " + path);
    }
    fh.seekg(0, std::ios::beg);
    // +1 for a trailing NUL so strtod has a guaranteed terminator.
    std::vector<char> buf(static_cast<size_t>(size) + 1);
    if (size > 0 && !fh.read(buf.data(), size)) {
        throw std::runtime_error("Failed to read curve file: " + path);
    }
    buf[static_cast<size_t>(size)] = '\0';

    const char* p = buf.data();
    const char* end = p + static_cast<size_t>(size);

    CurveFile out;

    expectChar(p, end, '{');
    bool firstField = true;
    while (!peekChar(p, end, '}')) {
        if (!firstField) expectChar(p, end, ',');
        firstField = false;

        const char* keyBeg;
        size_t keyLen;
        readRawString(p, end, keyBeg, keyLen);
        expectChar(p, end, ':');

        if (keyLen == 16 && std::memcmp(keyBeg, "measuredChannels", 16) == 0) {
            double v = readNumber(p, end);
            out.measuredChannels = static_cast<uint32_t>(v);
        } else if (keyLen == 6 && std::memcmp(keyBeg, "curves", 6) == 0) {
            expectChar(p, end, '{');
            bool firstCurve = true;
            while (!peekChar(p, end, '}')) {
                if (!firstCurve) expectChar(p, end, ',');
                firstCurve = false;

                int pct = readQuotedInt(p, end);
                expectChar(p, end, ':');
                expectChar(p, end, '[');

                if (pct < 0 || pct > 100) {
                    parseError("read percentage out of range");
                }

                CurveEntry entry;
                entry.readPercentage = pct;
                entry.points.reserve(64);

                bool firstPair = true;
                while (!peekChar(p, end, ']')) {
                    if (!firstPair) expectChar(p, end, ',');
                    firstPair = false;
                    expectChar(p, end, '[');
                    double bw = readNumber(p, end);
                    expectChar(p, end, ',');
                    double lat = readNumber(p, end);
                    expectChar(p, end, ']');
                    entry.points.emplace_back(bw, lat);
                }
                expectChar(p, end, ']');
                out.curves.push_back(std::move(entry));
            }
            expectChar(p, end, '}');
        } else {
            parseError("unknown top-level key");
        }
    }
    expectChar(p, end, '}');

    if (out.measuredChannels == 0) {
        throw std::runtime_error(
            "Curve file is missing a positive 'measuredChannels' field: " + path);
    }
    return out;
}

} // namespace

/**
 * @brief Constructs a MessMemCtrl object, initializing all internal states and
 *        loading bandwidth-latency curves from the specified JSON file.
 *
 * The constructor reads pre-characterized bandwidth-latency curves for various
 * read percentages from a JSON file, converts bandwidth and latency units, and
 * stores them for later use. Bandwidth values are linearly scaled by
 * ``channels / measuredChannels`` so curves measured on a system with a different
 * channel count can still be used.
 *
 * @param _curveAddress  Path to the curve JSON file.
 * @param _curveWindowSize Number of accesses per measurement window.
 * @param frequencyRate  CPU frequency in GHz.
 * @param _channels      Number of memory channels of the simulated system.
 */
MessMemCtrl::MessMemCtrl(const std::string& _curveAddress,
                           uint32_t _curveWindowSize, double frequencyRate,
                           uint32_t _channels)
    : curveAddress(_curveAddress),
      measuredChannels(0),
      channels(_channels),
      curveWindowSize(_curveWindowSize),
      frequencyCPU(frequencyRate),
      leadOffLatency(100000), // Initialize with a large value; will be updated later
      maxBandwidth(0),
      maxLatency(0),
      currentWindowAccessCount(0),
      currentWindowAccessCount_wr(0),
      currentWindowAccessCount_rd(0),
      lastEstimatedBandwidth(0),
      lastEstimatedLatency(leadOffLatency),
      latency(static_cast<uint32_t>(leadOffLatency)),
      overflowFactor(0),
      lastIntReadPercentage(0) {
    // Load the entire curve file (measuredChannels + curves) from JSON.
    CurveFile file = loadCurveFile(curveAddress);
    measuredChannels = file.measuredChannels;

    if (file.curves.empty()) {
        throw std::runtime_error("Curve file contains no curves: " + curveAddress);
    }

    // Bandwidth scaling factor: curves were measured on `measuredChannels`
    // channels; linearly rescale to match the simulated system's `channels`.
    const double bwScale =
        static_cast<double>(channels) / static_cast<double>(measuredChannels);

    // Load each curve from the JSON file, apply the channel-scaling factor
    // to bandwidth values, and convert units (MB/s → accesses per cycle,
    // ns → CPU cycles). The result is stored in `curves_data`, one entry
    // per curve found in the file.
    const size_t numCurves = file.curves.size();
    curves_data.reserve(numCurves);
    maxBandwidthPerRdRatio.reserve(numCurves);
    maxLatencyPerRdRatio.reserve(numCurves);

    for (const auto& entry : file.curves) {
        std::vector<std::vector<double>> curve_data;
        curve_data.reserve(entry.points.size());
        double maxBandwidthTemp = 0;
        double maxLatencyTemp = 0;

        for (const auto& pair : entry.points) {
            // Apply per-channel bandwidth scaling first (still in MB/s),
            // then convert from MB/s to accesses per cycle, assuming 64-byte accesses.
            double inputBandwidth = pair.first * bwScale;
            inputBandwidth = (inputBandwidth / 64) / (frequencyCPU * 1000);

            // Adjust input latency based on the CPU frequency
            // The input latency is in ns; convert it to the CPU's cycles
            double inputLatency = pair.second * frequencyCPU;

            // Store the data point (bandwidth, latency)
            curve_data.push_back({inputBandwidth, inputLatency});

            // Update lead-off latency (minimum latency)
            if (leadOffLatency > inputLatency)
                leadOffLatency = inputLatency;

            // Update maximum latency and bandwidth for all curves
            if (maxLatency < inputLatency)
                maxLatency = inputLatency;
            if (maxBandwidth < inputBandwidth)
                maxBandwidth = inputBandwidth;

            // Update maximum latency and bandwidth for the current curve
            if (maxLatencyTemp < inputLatency)
                maxLatencyTemp = inputLatency;
            if (maxBandwidthTemp < inputBandwidth)
                maxBandwidthTemp = inputBandwidth;
        }

        maxBandwidthPerRdRatio.push_back(maxBandwidthTemp);
        maxLatencyPerRdRatio.push_back(maxLatencyTemp);
        curves_data.push_back(std::move(curve_data));
    }

    // set initial latency to the lead-off latency
    lastEstimatedLatency = leadOffLatency;
    latency = static_cast<uint32_t>(leadOffLatency);

    // ------------------------------------------------------------------
    // Phase 2: build `pctToCurveIdx` so the hot path can map any integer
    // read percentage in [0, 100] to a curve index with a single load.
    //
    // The table is filled by:
    //   1. Marking each pct that has its own curve in the JSON.
    //   2. A left-to-right sweep that records the nearest pct seen so far
    //      on the left side of every slot.
    //   3. A right-to-left sweep doing the same for the right side.
    //   4. For each slot, picking whichever neighbour is closer (ties go
    //      to the lower pct, matching the prior round-half-down behaviour).
    // ------------------------------------------------------------------
    constexpr int kMaxPct = 100;
    std::vector<int32_t> ownerByPct(kMaxPct + 1, -1);
    for (size_t i = 0; i < file.curves.size(); ++i) {
        ownerByPct[file.curves[i].readPercentage] = static_cast<int32_t>(i);
    }

    std::vector<int> nearestLeft(kMaxPct + 1, -1);
    std::vector<int> nearestRight(kMaxPct + 1, -1);
    int last = -1;
    for (int p = 0; p <= kMaxPct; ++p) {
        if (ownerByPct[p] >= 0) last = p;
        nearestLeft[p] = last;
    }
    last = -1;
    for (int p = kMaxPct; p >= 0; --p) {
        if (ownerByPct[p] >= 0) last = p;
        nearestRight[p] = last;
    }

    pctToCurveIdx.assign(kMaxPct + 1, 0);
    for (int p = 0; p <= kMaxPct; ++p) {
        const int lp = nearestLeft[p];
        const int rp = nearestRight[p];
        int chosen;
        if (lp < 0)      chosen = rp;
        else if (rp < 0) chosen = lp;
        else             chosen = (p - lp <= rp - p) ? lp : rp;
        // chosen >= 0 here because file.curves is non-empty.
        pctToCurveIdx[p] = static_cast<uint32_t>(ownerByPct[chosen]);
    }
}

/**
 * @brief Returns the minimum achievable latency (lead-off latency).
 *
 * The lead-off latency is the lowest possible latency observed among all
 * loaded curves and serves as a baseline for the memory system.
 *
 * @return Lead-off latency in cycles.
 */
uint32_t MessMemCtrl::getLeadOffLatency() {
    // Return the minimum achievable latency for memory access
    return static_cast<uint32_t>(leadOffLatency);
}


/**
 * @brief Retrieves the peak bandwidth of the loaded curves, in GB/s.
 *
 * `maxBandwidth` is stored internally in accesses-per-cycle, after the
 * per-channel scaling factor has been applied at construction time. The
 * inverse of the conversion done in the constructor is:
 *
 *     accesses/cycle * 64 bytes * frequencyCPU [GHz] = GB/s
 *
 * @return Peak bandwidth in GB/s, already scaled to the simulated channel count.
 */
double MessMemCtrl::getPeakBandwidthGBs() const {
    return maxBandwidth * 64.0 * frequencyCPU;
}


/**
 * @brief Searches for the appropriate latency given a measured bandwidth and read percentage.
 *
 * This method maps the current bandwidth and read percentage to the corresponding
 * point on the pre-loaded bandwidth-latency curves. It employs a PID-like controller
 * to smoothly converge to the right latency value, preventing abrupt changes. If the
 * bandwidth exceeds a threshold, it applies an overflow penalty to simulate
 * increased contention.
 *
 * @param bandwidth Current measured bandwidth in accesses per cycle.
 * @param readPercentage Fraction of accesses that are reads, from 0.0 to 1.0.
 * @return Estimated latency in cycles for the given bandwidth and read percentage.
 */
uint32_t MessMemCtrl::searchForLatencyOnCurve(double bandwidth, double readPercentage) {
    const double convergeSpeed = 0.05; // Convergence factor for PID-like controller

    // Convert read percentage to an integer in [0, 100].
    int rp = static_cast<int>(roundDouble(100 * readPercentage));
    if (rp < 0)        rp = 0;
    else if (rp > 100) rp = 100;
    const uint32_t intReadPercentage = static_cast<uint32_t>(rp);
    lastIntReadPercentage = intReadPercentage;

    // O(1) lookup of the matching (or nearest) curve produced by the loader.
    const uint32_t curveDataIndex = pctToCurveIdx[intReadPercentage];

    // Initialize estimated data points
    double finalLatency = leadOffLatency;
    double finalBW = 0.0;

    // Apply a weighted average to bandwidth for smooth convergence (PID-like control)
    bandwidth = convergeSpeed * bandwidth + (1 - convergeSpeed) * lastEstimatedBandwidth;

    // Check if the bandwidth exceeds the maximum allowed for the current read percentage
    if (maxBandwidthPerRdRatio[curveDataIndex] * 0.99 < bandwidth) {
        // Limit the bandwidth to the maximum and apply a weighted average
        finalBW = convergeSpeed * maxBandwidthPerRdRatio[curveDataIndex] +
                  (1 - convergeSpeed) * lastEstimatedBandwidth;

        // Increase overflow factor to simulate latency penalty for high bandwidth
        overflowFactor += 0.02;

        // Calculate the latency penalty based on the overflow factor
        finalLatency = (1 + overflowFactor) * maxLatencyPerRdRatio[curveDataIndex];
        finalLatency = convergeSpeed * finalLatency + (1 - convergeSpeed) * lastEstimatedLatency;

        // Update the last estimated bandwidth and latency
        lastEstimatedBandwidth = finalBW;
        lastEstimatedLatency = finalLatency;

        // Ensure latency is not less than the lead-off latency
        if (finalLatency < leadOffLatency)
            finalLatency = leadOffLatency;

        // Sanity check
        assert(finalLatency >= leadOffLatency);
        return static_cast<uint32_t>(finalLatency);
    }

    // Find the appropriate latency corresponding to the current bandwidth
    uint32_t j;
    for (j = 0; j < curves_data[curveDataIndex].size(); ++j) {
        if (finalBW == 0) {
            // Initialize finalBW and finalLatency with the first data point
            finalBW = curves_data[curveDataIndex][j][0];
            finalLatency = curves_data[curveDataIndex][j][1];
        }
        if (curves_data[curveDataIndex][j][0] >= bandwidth) {
            // Update finalBW and finalLatency if the curve's bandwidth is greater than or equal to the current bandwidth
            finalBW = curves_data[curveDataIndex][j][0];
            finalLatency = curves_data[curveDataIndex][j][1];
        } else {
            // Break the loop when we find a bandwidth less than the current bandwidth
            break;
        }
    }

    // Adjust index if we've reached the end of the curve data
    if (j == curves_data[curveDataIndex].size())
        j--;

    if (j != 0) { // Not at the first data point (highest bandwidth)
        // Perform linear interpolation between two data points to estimate the latency
        double x1 = curves_data[curveDataIndex][j][0];
        double y1 = curves_data[curveDataIndex][j][1];
        double x2 = curves_data[curveDataIndex][j - 1][0];
        double y2 = curves_data[curveDataIndex][j - 1][1];
        double x = bandwidth;

        // Calculate the interpolated latency
        finalLatency = y1 + ((x - x1) / (x2 - x1)) * (y2 - y1);

        // Adjust latency with overflow factor to stabilize the system
        finalLatency += overflowFactor * finalLatency;

        // Apply a weighted average to latency for smooth convergence
        finalLatency = convergeSpeed * finalLatency + (1 - convergeSpeed) * lastEstimatedLatency;

        // Decrease overflow factor if not in overflow mode
        if (overflowFactor > 0.01)
            overflowFactor -= 0.01;
    } else {
        // At the first data point; use the current finalLatency
        finalLatency += overflowFactor * finalLatency;
        finalLatency = convergeSpeed * finalLatency + (1 - convergeSpeed) * lastEstimatedLatency;

        // Decrease overflow factor if not in overflow mode
        if (overflowFactor > 0.01)
            overflowFactor -= 0.01;
        if (overflowFactor < 0)
            overflowFactor = 0;
    }

    // Update the last estimated bandwidth and latency
    lastEstimatedBandwidth = bandwidth;
    lastEstimatedLatency = finalLatency;

    // Ensure latency is not less than the lead-off latency
    if (finalLatency <= leadOffLatency)
        finalLatency = leadOffLatency;

    // Sanity check
    assert(finalLatency >= leadOffLatency);

    return static_cast<uint32_t>(finalLatency);
}


/**
 * @brief Updates the latency at the end of a measurement window.
 *
 * After collecting a window's worth of memory accesses, this method calculates the
 * observed bandwidth and read percentage, then uses them to update the estimated
 * latency. It ensures that the latency value reflects the current memory load.
 *
 * @param currentWindowEndCycle The cycle number at which the current measurement window ends.
 */
void MessMemCtrl::updateLatency(uint64_t currentWindowEndCycle) {
    // Calculate bandwidth in accesses per cycle
    double bandwidth =
        static_cast<double>(currentWindowAccessCount) / (currentWindowEndCycle - currentWindowStartCycle);

    // Calculate read percentage
    double readPercentage =
        static_cast<double>(currentWindowAccessCount_rd) /
        (currentWindowAccessCount_rd + currentWindowAccessCount_wr);

    // Update latency based on the calculated bandwidth and read percentage
    latency = searchForLatencyOnCurve(bandwidth, readPercentage);

    // Sanity check
    assert(latency >= 0);
}

/**
 * @brief Simulates a memory access at a given cycle and returns its latency.
 *
 * Each access is recorded. Once the number of accesses reaches the window size,
 * the latency is recalculated based on the collected statistics. Subsequent accesses
 * will reflect any changes in the memory system conditions.
 *
 * @param accessCycle The cycle at which the memory access occurs.
 * @param isWrite True if the access is a write, false if it is a read.
 * @return Latency in cycles for this access.
 */
uint64_t MessMemCtrl::access(uint64_t accessCycle, bool isWrite) {
    // Start cycle of the measurement window
    if (currentWindowAccessCount == 0) {
        currentWindowStartCycle = accessCycle;
    }

    // Increment access counts
    currentWindowAccessCount++;
    if (isWrite) {
        currentWindowAccessCount_wr++;
    } else {
        currentWindowAccessCount_rd++;
    }

    // Check if we've reached the end of the measurement window
    if (currentWindowAccessCount == curveWindowSize) {
        // Update latency based on the current window's statistics
        updateLatency(accessCycle);

        // Reset counts for the new window
        currentWindowAccessCount = 0;
        currentWindowAccessCount_wr = 0;
        currentWindowAccessCount_rd = 0;
    }

    // Return the latency in cycles (CPU frequency)
    return static_cast<uint64_t>(latency);
}

/**
 * @brief Retrieves the additional latency penalty when exceeding maximum bandwidth.
 *
 * This method calculates how much the current latency surpasses the maximum latency
 * for the given read percentage. It helps model scenarios where bandwidth saturation
 * leads to latency penalties, aiding Quality of Service (QoS) simulations.
 *
 * @return The latency penalty in cycles if current latency exceeds max latency,
 *         otherwise 0.
 */
uint64_t MessMemCtrl::GetQsMemLoadCycleLimit() {
    // O(1) lookup of the curve index for the last read percentage observed.
    uint32_t curveDataIndex = pctToCurveIdx[lastIntReadPercentage];

    // Determine if there's a latency penalty due to bandwidth exceeding the maximum
    if (latency > static_cast<uint32_t>(maxLatencyPerRdRatio[curveDataIndex]))
        return latency - static_cast<uint32_t>(maxLatencyPerRdRatio[curveDataIndex]);
    else
        return 0;
}