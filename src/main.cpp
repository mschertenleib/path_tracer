#include "geometry.hpp"
#include "renderer.hpp"

#include <chrono>
#include <iostream>
#include <stdexcept>

#include <cstdlib>

namespace
{

class Timer
{
public:
    inline void start(const char *name) noexcept
    {
        m_name = name;
        m_start = std::chrono::steady_clock::now();
    }
    inline void stop()
    {
        const auto stop = std::chrono::steady_clock::now();
        std::cout
            << m_name << ": "
            << std::chrono::duration<double, std::milli>(stop - m_start).count()
            << " ms\n";
    }

private:
    const char *m_name {""};
    std::chrono::steady_clock::time_point m_start {};
};

} // namespace

int main(int argc, char *argv[])
{
    try
    {
        if (argc != 3)
        {
            std::cerr << "Usage: " << argv[0] << " <model.obj> <output.png>\n";
            return EXIT_FAILURE;
        }

        Timer t;

        t.start("create_renderer");
        auto renderer = create_renderer();
        t.stop();

        t.start("load_obj");
        const auto geometry = load_obj(argv[1]);
        t.stop();

        t.start("load_scene");
        load_scene(renderer, 1280, 720, geometry);
        t.stop();

        t.start("render");
        render(renderer);
        t.stop();

        t.start("write_to_png");
        write_to_png(renderer, argv[2]);
        t.stop();
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception thrown\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
