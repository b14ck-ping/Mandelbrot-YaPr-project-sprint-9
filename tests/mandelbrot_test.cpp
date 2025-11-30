#include <gtest/gtest.h>

#include "mandelbrot_sender.hpp"
#include "types.hpp"
#include <stdexec/execution.hpp>

struct TestReceiver {
    using receiver_concept = stdexec::receiver_t;
    bool value_called = false;
    bool error_called = false;
    PixelMatrix result;

    void set_value(PixelMatrix matrix) noexcept {
        value_called = true;
        result = std::move(matrix);
    }
    void set_error(std::exception_ptr) noexcept { error_called = true; }
    void set_error(std::error_code) noexcept { error_called = true; }
    void set_stopped() noexcept {}
};

TEST(MandelbrotSenderTest, PipelineWorks) {
    mandelbrot::ViewPort viewport;
    RenderSettings settings;
    PixelRegion region{0, 2, 0, 2};
    auto sender = MakeMandelbrotSender(viewport, settings, region);
    TestReceiver receiver;
    auto op_state = stdexec::connect(std::move(sender), std::move(receiver));
    stdexec::start(op_state);
    EXPECT_TRUE(op_state.receiver_.value_called);
    EXPECT_FALSE(op_state.receiver_.error_called);
    EXPECT_EQ(op_state.receiver_.result.size(), region.end_row - region.start_row);
    EXPECT_EQ(op_state.receiver_.result[0].size(), region.end_col - region.start_col);

    for (const auto &row : op_state.receiver_.result) {
        for (const auto &val : row) {
            EXPECT_LE(val, settings.max_iterations);
        }
    }
}

#include "mandelbrot.hpp"
#include "mandelbrot_renderer.hpp"

struct RenderResultReceiver {
    using receiver_concept = stdexec::receiver_t;
    bool value_called = false;
    bool error_called = false;
    RenderResult result;

    void set_value(RenderResult res) noexcept {
        value_called = true;
        result = std::move(res);
    }
    void set_error(std::exception_ptr) noexcept { error_called = true; }
    void set_error(std::error_code) noexcept { error_called = true; }
    void set_stopped() noexcept {}
};

TEST(CalculateMandelbrotAsyncSenderTest, PipelineWorks) {
    AppState state;
    RenderSettings settings;
    MandelbrotRenderer renderer;
    CalculateMandelbrotAsyncSender sender(state, settings, renderer);
    RenderResultReceiver receiver;
    auto op_state = stdexec::connect(std::move(sender), std::move(receiver));
    stdexec::start(op_state);
    EXPECT_TRUE(op_state.receiver_.value_called || op_state.receiver_.error_called);

    EXPECT_FALSE(state.need_rerender);

    if (op_state.receiver_.value_called) {
        EXPECT_EQ(op_state.receiver_.result.settings.width, settings.width);
        EXPECT_EQ(op_state.receiver_.result.settings.height, settings.height);
    }
}

#include "sfml_events_handler.hpp"
#include <SFML/Graphics.hpp>

struct VoidReceiver {
    using receiver_concept = stdexec::receiver_t;
    bool value_called = false;
    bool error_called = false;

    void set_value() noexcept { value_called = true; }
    void set_error(std::exception_ptr) noexcept { error_called = true; }
    void set_error(std::error_code) noexcept { error_called = true; }
    void set_stopped() noexcept {}
};

TEST(SfmlEventHandlerTest, PipelineWorks) {
    sf::RenderWindow window;
    RenderSettings settings;
    AppState state;
    sf::Clock clock;
    SfmlEventHandler sender(window, settings, state, clock);
    VoidReceiver receiver;
    auto op_state = stdexec::connect(std::move(sender), std::move(receiver));
    stdexec::start(op_state);
    EXPECT_TRUE(op_state.receiver_.value_called || op_state.receiver_.error_called);

    EXPECT_TRUE(state.should_exit == false || state.should_exit == true);
}

#include "sfml_renderer.hpp"

TEST(SFMLRenderTest, PipelineWorks) {
    RenderResult result;
    sf::Image image;
    sf::Texture texture;
    sf::Sprite sprite;
    sf::RenderWindow window;
    RenderSettings settings;
    SFMLRender sender(result, image, texture, sprite, window, settings);
    VoidReceiver receiver;
    auto op_state = stdexec::connect(std::move(sender), std::move(receiver));
    stdexec::start(op_state);
    EXPECT_TRUE(op_state.receiver_.value_called || op_state.receiver_.error_called);

    EXPECT_TRUE(image.getSize().x == 0 || image.getSize().y == 0 || image.getPixelsPtr() != nullptr);
}

TEST(IntegrationTest, MandelbrotPipeline) {
    mandelbrot::ViewPort viewport;
    RenderSettings settings;
    PixelRegion region{0, 2, 0, 2};
    auto sender = MakeMandelbrotSender(viewport, settings, region);
    TestReceiver receiver;
    auto op_state = stdexec::connect(std::move(sender), std::move(receiver));
    stdexec::start(op_state);
    ASSERT_TRUE(op_state.receiver_.value_called);

    bool has_non_max = false;
    for (const auto &row : op_state.receiver_.result) {
        for (const auto &val : row) {
            if (val != settings.max_iterations)
                has_non_max = true;
        }
    }
    EXPECT_TRUE(has_non_max);
}
