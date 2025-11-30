#pragma once

#include <exec/static_thread_pool.hpp>
#include <stdexec/execution.hpp>
#include <tuple>
#include <utility>

#include "mandelbrot_sender.hpp"
#include "types.hpp"

class MandelbrotRenderer {
private:
    exec::static_thread_pool thread_pool_;

public:
    explicit MandelbrotRenderer(std::uint32_t num_threads = std::thread::hardware_concurrency())
        : thread_pool_{num_threads} {}

    template <size_t N>
    [[nodiscard]] auto RenderAsync(mandelbrot::ViewPort viewport, RenderSettings settings) {
        auto start_time = std::chrono::steady_clock::now();

        auto strip_sender = [this, viewport, settings](std::size_t i) {
            static_assert(N > 0);
            const auto rows = settings.height / N;
            bool is_last_row = (i == N - 1);
            PixelRegion region{
                .start_row = static_cast<std::uint32_t>(i * rows),
                .end_row = is_last_row ? settings.height : static_cast<std::uint32_t>((i + 1) * rows),
                .start_col = 0,
                .end_col = settings.width  //
            };
            return MakeMandelbrotSender(viewport, settings, region) |
                   stdexec::continues_on(thread_pool_.get_scheduler()) |
                   stdexec::then([max_iter = settings.max_iterations](const PixelMatrix &matrix) {
                       const auto height = matrix.size();
                       if (height == 0) {
                           return ColorMatrix{};
                       }
                       const auto width = matrix[0].size();
                       ColorMatrix colors(height, std::vector<mandelbrot::RgbColor>(width));
                       for (auto y = 0; y < height; ++y) {
                           for (auto x = 0; x < width; ++x) {
                               colors[y][x] = mandelbrot::IterationsToColor(matrix[y][x], max_iter);
                           }
                       }
                       return colors;
                   });
        };

        auto create_combined_sender = [&]<std::size_t... Is>(std::index_sequence<Is...>) {
            return stdexec::when_all(strip_sender(Is)...);
        };

        return create_combined_sender(std::make_index_sequence<N>{}) |
               stdexec::then([start_time, viewport, settings](auto &&...color_strips_pack) {
                   RenderResult result;
                   result.color_data.reserve(settings.height);
                   result.viewport = viewport;
                   result.settings = settings;

                   auto colors = std::make_tuple(std::forward<ColorMatrix>(color_strips_pack)...);

                   std::apply(
                       [&](auto &&...color) {
                           (result.color_data.insert(result.color_data.end(),  //
                                                     std::make_move_iterator(color.begin()),
                                                     std::make_move_iterator(color.end())),
                            ...);
                       },
                       std::move(colors));

                   auto end_time = std::chrono::steady_clock::now();
                   result.render_time = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

                   return result;
               });
    }
};
