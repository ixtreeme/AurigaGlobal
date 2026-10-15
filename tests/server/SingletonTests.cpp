#include <common/singleton.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <type_traits>

#ifdef _WIN32
#define popen _popen
#define pclose _pclose
#endif

// common/singleton.h as the release build sees it: one live instance,
// registered at the derived object's address even behind another base,
// cleared on destruction, never copied. A second instance or a use while
// none is alive must stop the process with the type's name - checked by
// running this binary again on each failure scenario.
namespace {
int checks = 0;
void Check(bool value, const char* why) { ++checks; if (!value) throw std::runtime_error(why); }

struct Probe : singleton<Probe> { int value = 7; };
struct Pad { virtual ~Pad() = default; char bytes[24] {}; };
struct Behind : Pad, singleton<Behind> { int value = 9; };

static_assert(!std::is_copy_constructible_v<Probe> && !std::is_copy_assignable_v<Probe>);
static_assert(!std::is_move_constructible_v<Probe> && !std::is_move_assignable_v<Probe>);

void Lifetime()
{
    Check(Probe::instance_ptr() == nullptr, "an instance existed before construction");
    {
        Probe probe;
        Check(&Probe::instance() == &probe && &Probe::Instance() == &probe && Probe::instance_ptr() == &probe,
            "the registered instance is not the constructed object");
        Check(Probe::instance().value == 7, "instance() read the wrong object");
    }
    Check(Probe::instance_ptr() == nullptr, "a destroyed instance stayed registered");
    {
        Probe again;
        Check(&Probe::instance() == &again, "a later instance did not register");
    }
    Check(Probe::instance_ptr() == nullptr, "the later instance stayed registered");

    Behind behind;
    Check(static_cast<void*>(&behind) != static_cast<void*>(static_cast<singleton<Behind>*>(&behind)),
        "fixture: the singleton base must not sit at offset zero");
    Check(&Behind::instance() == &behind && Behind::instance().value == 9,
        "an instance behind another base registered the wrong address");
}

int Scenario(const std::string& name)
{
#ifdef _WIN32
    // abort() must end the child quietly, without an error-reporting dialog.
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    if (name == "second") {
        Probe first;
        Probe second;
    } else if (name == "none") {
        std::printf("%d\n", Probe::instance().value);
    } else if (name == "after") {
        { Probe gone; }
        std::printf("%d\n", Probe::instance().value);
    } else {
        std::fprintf(stderr, "unknown scenario %s\n", name.c_str());
        return 2;
    }
    std::fflush(stdout);
    return 0;
}

int RunChild(const std::string& self, const char* scenario, std::string& output)
{
    std::string command = "\"" + self + "\" " + scenario + " 2>&1";
#ifdef _WIN32
    command = "\"" + command + "\"";  // cmd /c strips the outer pair
#endif
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe)
        throw std::runtime_error("could not start the child process");
    char chunk[256];
    while (std::fgets(chunk, sizeof(chunk), pipe))
        output += chunk;
    return pclose(pipe);
}

void Failures(const std::string& self)
{
    const struct { const char* scenario; const char* reason; } cases[] = {
        {"second", "a second instance was constructed"},
        {"none", "used while no instance exists"},
        {"after", "used while no instance exists"},
    };
    for (const auto& c : cases) {
        std::string output;
        const int status = RunChild(self, c.scenario, output);
        if (status == 0 || output.find("singleton<") == std::string::npos ||
            output.find("Probe") == std::string::npos || output.find(c.reason) == std::string::npos)
            std::cerr << "scenario " << c.scenario << " exit " << status << " output: " << output << '\n';
        Check(status != 0, "the process survived a singleton misuse");
        Check(output.find("singleton<") != std::string::npos && output.find("Probe") != std::string::npos &&
            output.find(c.reason) != std::string::npos, "the misuse did not name the type and the reason");
    }
}
}

int main(int argc, char** argv)
{
    if (argc == 2)
        return Scenario(argv[1]);
    try {
        Lifetime();
        Failures(argv[0]);
    } catch (const std::exception& error) {
        std::cerr << "singleton test failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "singleton: " << checks << " checks passed\n";
    return 0;
}
