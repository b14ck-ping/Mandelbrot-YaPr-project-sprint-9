#pragma once

#include <SFML/Graphics.hpp>
#include <print>
#include <stdexec/execution.hpp>

#include "types.hpp"

class SFMLRender {
public:
    using sender_concept = stdexec::sender_t;

    using completion_signatures = stdexec::completion_signatures<  //
        stdexec::set_value_t(),                                    //
        stdexec::set_error_t(std::exception_ptr),                  //
        stdexec::set_stopped_t()                                   //
        >;

    template <typename Receiver>
    struct OperationState {
        Receiver receiver_;
        RenderResult render_result_;
        sf::Image &image_;
        sf::Texture &texture_;
        sf::Sprite &sprite_;
        sf::RenderWindow &window_;
        RenderSettings render_settings_;

        template <typename R>
        OperationState(R &&rec, RenderResult res, sf::Image &img, sf::Texture &tex, sf::Sprite &spr,
                       sf::RenderWindow &win, RenderSettings settings)
            : receiver_(std::forward<R>(rec)), render_result_(std::move(res)), image_{img}, texture_{tex}, sprite_{spr},
              window_{win}, render_settings_{settings} {}

        friend void tag_invoke(stdexec::start_t, OperationState &self) noexcept {
            try {
                if (self.render_result_.color_data.empty()) {
                    stdexec::set_value(std::move(self.receiver_));
                    return;
                }
                std::size_t height = self.render_result_.color_data.size();
                std::size_t width = height > 0 ? self.render_result_.color_data[0].size() : 0;

                if (width == 0 || height == 0) {
                    stdexec::set_error(std::move(self.receiver_), std::make_error_code(std::errc::invalid_argument));
                    return;
                }

                self.image_.create(width, height, sf::Color::Black);

                for (std::size_t y = 0; y < height; ++y) {
                    if (y >= self.render_result_.color_data.size()) {
                        break;
                    }

                    for (std::size_t x = 0; x < width; ++x) {
                        if (x >= self.render_result_.color_data[y].size()) {
                            break;
                        }

                        const auto &rgb_color = self.render_result_.color_data[y][x];
                        sf::Color sf_color(rgb_color.r, rgb_color.g, rgb_color.b);
                        self.image_.setPixel(x, y, sf_color);
                    }
                }

                if (!self.texture_.loadFromImage(self.image_)) {
                    throw std::runtime_error("Failed to load texture from image");
                }

                self.sprite_.setTexture(self.texture_, true);

                self.window_.clear();
                self.window_.draw(self.sprite_);
                self.window_.display();

                stdexec::set_value(std::move(self.receiver_));
            } catch (...) {
                stdexec::set_error(std::move(self.receiver_), std::current_exception());
            }
        }

    private:
    };

    RenderResult render_result_;
    sf::Image &image_;
    sf::Texture &texture_;
    sf::Sprite &sprite_;
    sf::RenderWindow &window_;
    RenderSettings render_settings_;

    SFMLRender(RenderResult render_result, sf::Image &image, sf::Texture &texture, sf::Sprite &sprite,
               sf::RenderWindow &window, RenderSettings render_settings)
        : render_result_(render_result), image_{image}, texture_{texture}, sprite_{sprite}, window_{window},
          render_settings_{render_settings} {}

    template <stdexec::receiver Receiver>
    friend auto tag_invoke(stdexec::connect_t, SFMLRender &&self, Receiver &&receiver)
        -> OperationState<std::decay_t<Receiver>> {
        return OperationState<std::decay_t<Receiver>>{std::forward<Receiver>(receiver),
                                                      std::move(self.render_result_),
                                                      self.image_,
                                                      self.texture_,
                                                      self.sprite_,
                                                      self.window_,
                                                      self.render_settings_};
    }
};
