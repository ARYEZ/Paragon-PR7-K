#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// (1.2.0) A RAR archive read by dmc_unrar (source/thirdparty/dmc_unrar), behind a file of its own: dmc_unrar's
// header brings in <windows.h> on Windows, whose macros (min, max, ...) mustn't meet JUCE's code. Paths and names
// are UTF-8.
class RarArchive
{
public:
    RarArchive();
    ~RarArchive();
    RarArchive (const RarArchive&) = delete;
    RarArchive& operator= (const RarArchive&) = delete;

    // Opens it; `stop` (polled while reading) cancels. Empty when it opened, else why not.
    std::string open (const std::string& pathUtf8, std::function<bool()> stop = {});
    std::size_t count() const;
    std::string name (std::size_t index) const;   // as stored ('\' or '/' between folders)
    bool isFolder (std::size_t index) const;
    std::uint64_t size (std::size_t index) const;
    // Unpacks one file, handing its bytes to `write` in pieces (false from it stops); its CRC is checked.
    // Empty when it unpacked, else why not ("cancelled" when `stop` said so).
    std::string extract (std::size_t index, const std::function<bool (const void*, std::size_t)>& write);

private:
    struct State;
    std::unique_ptr<State> state;
};
