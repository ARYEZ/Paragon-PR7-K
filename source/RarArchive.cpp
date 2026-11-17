#include "RarArchive.h"

#if defined (_WIN32) && ! defined (NOMINMAX)
 #define NOMINMAX
#endif
#define DMC_UNRAR_HEADER_FILE_ONLY
#include "thirdparty/dmc_unrar/dmc_unrar.c"

#include <vector>

struct RarArchive::State
{
    dmc_unrar_archive archive {};
    bool opened = false;
    std::function<bool()> stop;
    const std::function<bool (const void*, std::size_t)>* write = nullptr;
    bool writeFailed = false;
};

RarArchive::RarArchive() : state (std::make_unique<State>()) {}

RarArchive::~RarArchive()
{
    if (state->opened)
        dmc_unrar_archive_close (&state->archive);
}

std::string RarArchive::open (const std::string& path, std::function<bool()> stop)
{
    if (dmc_unrar_archive_init (&state->archive) != DMC_UNRAR_OK)
        return "couldn't start the RAR reader";
    state->stop = std::move (stop);
    state->archive.cancel.func = [] (void* opaque) { auto* s = (State*) opaque; return ! (s->stop && s->stop()); };
    state->archive.cancel.opaque = state.get();
    const auto result = dmc_unrar_archive_open_path (&state->archive, path.c_str());
    if (result != DMC_UNRAR_OK)
        return dmc_unrar_strerror (result);
    state->opened = true;
    return {};
}

std::size_t RarArchive::count() const { return state->opened ? (std::size_t) dmc_unrar_get_file_count (&state->archive) : 0; }

std::string RarArchive::name (std::size_t index) const
{
    const auto length = dmc_unrar_get_filename (&state->archive, index, nullptr, 0);
    if (length == 0)
        return {};
    std::vector<char> text ((size_t) length + 1, 0);
    if (dmc_unrar_get_filename (&state->archive, index, text.data(), length) == 0)
        return {};
    return text.data();
}

bool RarArchive::isFolder (std::size_t index) const { return dmc_unrar_file_is_directory (&state->archive, index); }

std::uint64_t RarArchive::size (std::size_t index) const
{
    const auto* stat = dmc_unrar_get_file_stat (&state->archive, index);
    return stat != nullptr ? (std::uint64_t) stat->uncompressed_size : 0;
}

std::string RarArchive::extract (std::size_t index, const std::function<bool (const void*, std::size_t)>& write)
{
    if (const auto supported = dmc_unrar_file_is_supported (&state->archive, index); supported != DMC_UNRAR_OK)
        return dmc_unrar_strerror (supported);
    std::vector<char> buffer (1 << 16);
    state->write = &write;
    state->writeFailed = false;
    const auto result = dmc_unrar_extract_file_with_callback (&state->archive, index, buffer.data(), buffer.size(), nullptr, true,
        state.get(),
        [] (void* opaque, void** piece, dmc_unrar_size_t*, dmc_unrar_size_t n, dmc_unrar_return* err)
        {
            auto* s = (State*) opaque;
            if (! (*s->write) (*piece, (std::size_t) n))
            {
                s->writeFailed = true;
                *err = DMC_UNRAR_WRITE_FAIL;
                return false;
            }
            return true;
        });
    state->write = nullptr;
    if (result == DMC_UNRAR_OK)
        return {};
    if (result == DMC_UNRAR_USER_CANCEL)
        return "cancelled";
    if (state->writeFailed)
        return "write failed";
    return dmc_unrar_strerror (result);
}
