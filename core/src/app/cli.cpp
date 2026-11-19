#include "app/cli.hpp"

#include <charconv>
#include <cstddef>
#include <stdexcept>
#include <system_error>

namespace simcore {

namespace {

constexpr int kMaxPort = 65535;

double parseNumber(const std::string& text, const std::string& option) {
    double value = 0.0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        throw std::invalid_argument("invalid number '" + text + "' for " + option);
    }
    return value;
}

class ArgumentReader {
public:
    explicit ArgumentReader(const std::vector<std::string>& arguments) : arguments_(arguments) {}
    [[nodiscard]] bool done() const { return index_ >= arguments_.size(); }
    const std::string& next() { return arguments_[index_++]; }
    const std::string& value(const std::string& option) {
        if (done()) {
            throw std::invalid_argument(option + " needs a value");
        }
        return next();
    }

private:
    const std::vector<std::string>& arguments_;
    std::size_t index_ = 0;
};

} // namespace

CliParseResult parseCommandLine(const std::vector<std::string>& arguments) {
    CliOptions options;
    try {
        ArgumentReader reader(arguments);
        while (!reader.done()) {
            const std::string& option = reader.next();
            if (option == "--help" || option == "-h") {
                options.mode = CliMode::Help;
            } else if (option == "--version") {
                options.mode = CliMode::Version;
            } else if (option == "--engine") {
                options.engineId = reader.value(option);
            } else if (option == "--shared") {
                options.sharedDir = reader.value(option);
            } else if (option == "--port") {
                const double port = parseNumber(reader.value(option), option);
                if (port < 1 || port > kMaxPort || port != static_cast<int>(port)) {
                    throw std::invalid_argument("--port must be 1..65535");
                }
                options.port = static_cast<int>(port);
            } else if (option == "--sweep") {
                options.mode = CliMode::Sweep;
                options.sweepFrom_rpm = parseNumber(reader.value(option), option);
                options.sweepTo_rpm = parseNumber(reader.value(option), option);
            } else if (option == "--sound-sweep") {
                options.mode = CliMode::SoundSweep;
                options.soundSweepFile = reader.value(option);
                options.sweepFrom_rpm = kSoundSweepFrom_rpm;
                options.sweepTo_rpm = kSoundSweepTo_rpm;
            } else if (option == "--ignition-offset") {
                options.ignitionOffset_deg = parseNumber(reader.value(option), option);
            } else if (option == "--no-audio") {
                options.audio = false;
            } else if (option == "--rate") {
                options.sweepRate_rpm_s = parseNumber(reader.value(option), option);
            } else {
                throw std::invalid_argument("unknown option '" + option + "'");
            }
        }
    } catch (const std::invalid_argument& error) {
        return {std::nullopt, error.what()};
    }
    return {options, {}};
}

std::string usageText() {
    return "usage:\n"
           "  simcore [--engine <id>] [--port <n>]        serve the WebSocket API on 127.0.0.1\n"
           "  simcore --engine <id> --sweep <from> <to> [--rate <rpm/s>]\n"
           "                                             full-load dyno sweep as CSV on stdout\n"
           "  simcore --sound-sweep <file.wav> [--ignition-offset <deg>] [--rate <rpm/s>]\n"
           "                                             engine sound of a full-load sweep\n"
           "  simcore --version | --help\n"
           "options:\n"
           "  --shared <dir>   folder with engines/ and tables/ (default: repository shared/)\n"
           "  --no-audio       serve without opening the audio output device\n";
}

} // namespace simcore
