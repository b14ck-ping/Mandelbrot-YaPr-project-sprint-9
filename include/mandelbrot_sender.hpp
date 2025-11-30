#pragma once

#include <stdexec/execution.hpp>

#include "types.hpp"

template <typename Receiver>
struct MandelbrotOperationState {

    Receiver receiver_;
    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;

    friend void tag_invoke(stdexec::start_t, MandelbrotOperationState &self) noexcept {
        try {
            const auto height = self.region_.end_row - self.region_.start_row;
            const auto width = self.region_.end_col - self.region_.start_col;

            PixelMatrix iterations(height, std::vector<std::uint32_t>(width));

            for (auto y = self.region_.start_row; y < self.region_.end_row; ++y) {
                for (auto x = self.region_.start_col; x < self.region_.end_col; ++x) {
                    const auto complex =
                        mandelbrot::Pixel2DToComplex(x, y, self.viewport_, self.settings_.width, self.settings_.height);

                    const auto point_iterations = mandelbrot::CalculateIterationsForPoint(
                        complex, self.settings_.max_iterations, self.settings_.escape_radius);

                    iterations[y - self.region_.start_row][x - self.region_.start_col] = point_iterations;
                }
            }

            stdexec::set_value(std::move(self.receiver_), std::move(iterations));

        } catch (...) {
            stdexec::set_error(std::move(self.receiver_), std::current_exception());
        }
    }
};

struct MandelbrotSender {
    using sender_concept = stdexec::sender_t;

    using completion_signatures = stdexec::completion_signatures<  //
        stdexec::set_value_t(PixelMatrix),                         //
        stdexec::set_error_t(std::exception_ptr),                  //
        stdexec::set_stopped_t()                                   //
        >;

    mandelbrot::ViewPort viewport_;
    RenderSettings settings_;
    PixelRegion region_;

    template <stdexec::receiver Receiver>
    friend auto tag_invoke(stdexec::connect_t, MandelbrotSender &&self, Receiver &&receiver)
        -> MandelbrotOperationState<std::decay_t<Receiver>> {
        return MandelbrotOperationState<std::decay_t<Receiver>>{std::forward<Receiver>(receiver),  //
                                                                self.viewport_,                    //
                                                                self.settings_,                    //
                                                                self.region_};
    }
};

[[nodiscard]] inline auto MakeMandelbrotSender(mandelbrot::ViewPort viewport, RenderSettings settings,
                                               PixelRegion region) {
    return MandelbrotSender{viewport, settings, region};
}
