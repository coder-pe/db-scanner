#include "config/CliArgs.hpp"

#include <charconv>
#include <sstream>

namespace dbscanner::config {

namespace {
std::string nextValue(int argc, char** argv, int& i, const std::string& flag) {
    if (i + 1 >= argc) {
        throw CliParseError("missing value for " + flag);
    }
    return argv[++i];
}

int parseInt(const std::string& text, const std::string& flag) {
    int value = 0;
    const auto* begin = text.data();
    const auto* end = text.data() + text.size();
    auto result = std::from_chars(begin, end, value);
    if (result.ec != std::errc{} || result.ptr != end) {
        throw CliParseError("invalid integer value for " + flag + ": '" + text + "'");
    }
    return value;
}

std::vector<std::string> splitCsv(const std::string& text) {
    std::vector<std::string> parts;
    std::stringstream ss(text);
    std::string item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) parts.push_back(item);
    }
    return parts;
}
}  // namespace

CliArgs parseCliArgs(int argc, char** argv) {
    CliArgs args;

    for (int i = 1; i < argc; ++i) {
        const std::string flag = argv[i];

        if (flag == "--help" || flag == "-h") {
            args.showHelp = true;
        } else if (flag == "--version") {
            args.showVersion = true;
        } else if (flag == "--connect" || flag == "-c") {
            args.connectString = nextValue(argc, argv, i, flag);
        } else if (flag == "--user" || flag == "-u") {
            args.username = nextValue(argc, argv, i, flag);
        } else if (flag == "--schema") {
            args.schemaOwner = nextValue(argc, argv, i, flag);
        } else if (flag == "--config") {
            args.configPath = nextValue(argc, argv, i, flag);
        } else if (flag == "--output" || flag == "-o") {
            args.outputDir = nextValue(argc, argv, i, flag);
        } else if (flag == "--threads" || flag == "-j") {
            args.threads = parseInt(nextValue(argc, argv, i, flag), flag);
        } else if (flag == "--resume") {
            args.resume = true;
        } else if (flag == "--log-level") {
            args.logLevel = nextValue(argc, argv, i, flag);
        } else if (flag == "--sample-size") {
            args.sampleSize = parseInt(nextValue(argc, argv, i, flag), flag);
        } else if (flag == "--exclude") {
            for (const auto& pattern : splitCsv(nextValue(argc, argv, i, flag))) {
                args.excludeTablePatterns.push_back(pattern);
            }
        } else if (flag == "--no-infer") {
            args.noInfer = true;
        } else {
            throw CliParseError("unknown flag: " + flag);
        }
    }

    return args;
}

std::string usageText() {
    return R"(db-scanner - Oracle schema, dependency and data-consistency scanner

USAGE:
  db-scanner --connect <host:port/service> --user <username> [OPTIONS]

REQUIRED:
  -c, --connect <str>     Oracle easy-connect string or TNS alias
  -u, --user <str>        Oracle username

OPTIONS:
  --schema <str>          Schema/owner to scan (defaults to --user)
  --config <path>         JSON config file (CLI flags override file values)
  -o, --output <dir>      Output directory for reports/checkpoint (default: ./dbscanner-out)
  -j, --threads <n>       Worker thread pool size (default: 4)
  --resume                Resume from an interrupted run found in --output
  --log-level <level>     trace|debug|info|warn|error (default: info)
  --sample-size <n>       Max sample rows kept per consistency finding (default: 20)
  --exclude <a,b,c>       Comma-separated table name glob patterns to skip
  --no-infer              Only use declared foreign keys, skip name-based inference
  -h, --help              Show this help
  --version               Show version

ENVIRONMENT:
  DBSCANNER_ORACLE_PWD    Oracle password (if unset, prompted interactively)
)";
}

std::string versionText() { return "db-scanner 0.1.0"; }

}  // namespace dbscanner::config
