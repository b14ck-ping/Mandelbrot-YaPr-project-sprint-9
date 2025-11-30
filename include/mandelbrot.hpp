#pragma once

#include "mandelbrot_renderer.hpp"

class CalculateMandelbrotAsyncSender {
public:
    using sender_concept = stdexec::sender_t;

    using completion_signatures = stdexec::completion_signatures<  //
        stdexec::set_value_t(RenderResult),                        //
        stdexec::set_error_t(std::exception_ptr),                  //
        stdexec::set_stopped_t()                                   //
        >;

    template <typename Receiver>
    struct OperationState {
        Receiver receiver_;
        AppState &state_;
        RenderSettings render_settings_;
        MandelbrotRenderer &renderer_;

        template <typename R>
        explicit OperationState(R &&r, AppState &state, RenderSettings render_settings, MandelbrotRenderer &renderer)
            : receiver_{std::forward<R>(r)}, state_{state}, render_settings_{render_settings}, renderer_{renderer} {}

        friend void tag_invoke(stdexec::start_t, OperationState &self) noexcept {
            try {
                if (!self.state_.need_rerender) {
                    stdexec::set_value(std::move(self.receiver_), RenderResult{});
                    return;
                }
                self.state_.need_rerender = false;

                auto render_sender =
                    self.renderer_.RenderAsync<THREAD_POOL_SIZE>(self.state_.viewport, self.render_settings_);

                auto render_result = stdexec::sync_wait(std::move(render_sender));

                if (!render_result.has_value()) {
                    std::logic_error error("Rendering failed");
                    stdexec::set_error(std::move(self.receiver_), std::make_exception_ptr(error));
                    return;
                }

                auto &[result] = render_result.value();
                stdexec::set_value(std::move(self.receiver_), std::move(result));

            } catch (...) {
                stdexec::set_error(std::move(self.receiver_), std::current_exception());
            }
        }
    };

    explicit CalculateMandelbrotAsyncSender(AppState &state, RenderSettings render_settings,
                                            MandelbrotRenderer &renderer)
        : state_(state), render_settings_{render_settings}, renderer_{renderer} {}

    template <stdexec::receiver Receiver>
    friend auto tag_invoke(stdexec::connect_t, CalculateMandelbrotAsyncSender &&self, Receiver &&receiver)
        -> OperationState<std::decay_t<Receiver>> {
        using Op = OperationState<std::decay_t<Receiver>>;
        return Op{std::forward<Receiver>(receiver), self.state_, self.render_settings_, self.renderer_};
    }

private:
    RenderSettings render_settings_;
    MandelbrotRenderer &renderer_;
    AppState &state_;
};
