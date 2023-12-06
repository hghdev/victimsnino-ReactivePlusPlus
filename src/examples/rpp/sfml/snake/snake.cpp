#include "snake.hpp"
#include "canvas.hpp"
#include "utils.hpp"

#include <SFML/Graphics.hpp>
#include <rpp/rpp.hpp>

#include <random>

static SnakeBody generate_initial_snake_body()
{
    // tail to head
    return SnakeBody{ Coordinates{1,1}, Coordinates{2,1}, Coordinates{3,1}, Coordinates{4,1} };
}

static Coordinates generate_initial_apple()
{
    return Coordinates{ 3,5 };
}

static int wrap_coordinate(int value, int max_value)
{
    return value < 0 ? max_value : value > max_value ? 0 : value;
}

static SnakeBody move_snake(SnakeBody&& body, const std::tuple<Direction, size_t>& direction_and_length)
{
    auto head = body.back();

    const auto& [direction, length] = direction_and_length;

    if (length == body.size()) // no any changes
        std::rotate(body.begin(), body.begin() + 1, body.end());
    else
        body.push_back(head);

    head.x += direction.x;
    head.x = wrap_coordinate(head.x, s_columns_count);

    head.y += direction.y;
    head.y = wrap_coordinate(head.y, s_rows_count);

    body.back() = head;

    return std::move(body);
}

static bool is_snake_eat_self(const SnakeBody& body)
{
    const auto& head = body.back();

    return std::find(body.cbegin(), body.cend() - 1, head) == body.cend() - 1;
}

static Coordinates update_apple_position_if_eat(Coordinates&& apple_position, const SnakeBody& snake)
{
    if (std::find(snake.cbegin(), snake.cend(), apple_position) == snake.cend())
        return apple_position;

    static std::random_device rd;
    static std::mt19937 rng(rd()); 
    static std::uniform_int_distribution<int> x_uni(0, s_columns_count-1); 
    static std::uniform_int_distribution<int> y_uni(0, s_rows_count-1); 

    return Coordinates{ x_uni(rng), y_uni(rng) };
}

static Direction select_next_not_opposite_direction(Direction&& current_direction, const Direction& new_direction)
{
    if (current_direction.x == new_direction.x * -1 || current_direction.y ==
        new_direction.y * -1)
        return current_direction;
    return new_direction;
}
rpp::dynamic_observable<sf::RectangleShape> get_shapes_to_draw(const rpp::dynamic_observable<CustomEvent>& events)
{
    const auto key_event = events | rpp::ops::filter([](const CustomEvent& ev) { return std::holds_alternative<sf::Event>(ev); })
                                  | rpp::ops::map([](const CustomEvent& ev) { return std::get<sf::Event>(ev); })
                                  | rpp::ops::filter([](const sf::Event& event) { return event.type == sf::Event::KeyPressed; })
                                  | rpp::ops::map([](const sf::Event& event) { return event.key; });

    static const std::map<sf::Keyboard::Key, Direction> s_key_to_direction
    {
        {sf::Keyboard::Key::Right, { 1,  0}},
        {sf::Keyboard::Key::Left,  {-1,  0}},
        {sf::Keyboard::Key::Down,  { 0,  1}},
        {sf::Keyboard::Key::Up,    { 0, -1}},
    };

    const auto initial_direction = s_key_to_direction.at(sf::Keyboard::Key::Right);
    auto direction = key_event | rpp::ops::filter([](const sf::Event::KeyEvent& key_event)
                              {
                                  return !key_event.alt && !key_event.control && !key_event.shift && !key_event.system;
                              })
                               | rpp::ops::map([](const sf::Event::KeyEvent& event)-> std::optional<Direction>
                              {
                                  const auto itr = s_key_to_direction.find(event.code);
                                  if (itr != s_key_to_direction.cend())
                                      return itr->second;
                                  return std::nullopt;
                              })
                               | rpp::ops::filter([](const auto& optional) { return optional.has_value(); })
                               | rpp::ops::map([](const auto&    optional) { return optional.value(); })
                               | rpp::ops::start_with(initial_direction);

    auto initial_snake_body = generate_initial_snake_body();

    const auto snake_earn_points       = rpp::subjects::publish_subject<size_t>{};
    auto snake_length_observable = snake_earn_points
                                   .get_observable()
                                   | rpp::ops::scan(initial_snake_body.size(),
                                         [](size_t seed, size_t new_points)
                                         {
                                             return seed + new_points;
                                         });

    const auto snake_body = rpp::source::interval(std::chrono::milliseconds{200}, g_run_loop)
                          | rpp::ops::with_latest_from([](const auto&, const auto& second){ return second; }, std::move(direction))
                          | rpp::ops::scan(initial_direction, &select_next_not_opposite_direction)
                          | rpp::ops::with_latest_from(std::move(snake_length_observable))
                          | rpp::ops::scan(std::move(initial_snake_body), &move_snake)
                          | rpp::ops::take_while(&is_snake_eat_self)
                          | rpp::ops::publish()
                          | rpp::ops::ref_count();

    auto apple_position = snake_body
                        | rpp::ops::scan(generate_initial_apple(), update_apple_position_if_eat)
                        | rpp::ops::publish()
                        | rpp::ops::ref_count();

    static constexpr size_t points_per_apple = 1;
    apple_position | rpp::ops::distinct_until_changed()
                   | rpp::ops::skip(1) // skip first apple position to avoid snake growing immediately
                   | rpp::ops::map([](const auto&) { return points_per_apple; })
                   | rpp::ops::subscribe(snake_earn_points.get_observer());

    auto drawable_objects = snake_body | rpp::ops::with_latest_from([](const SnakeBody& body, const Coordinates& apple_coords)
    {
        return rpp::source::from_iterable(body)
               | rpp::ops::map([](const Coordinates& coords) { return get_rectangle_at(coords, sf::Color::White); })
               | rpp::ops::merge_with(rpp::source::just(apple_coords) | rpp::ops::map([](const Coordinates& coords)
                            {
                                return get_rectangle_at(coords, sf::Color::Red);
                            }));
    }, apple_position);

    return get_presents_stream(events)
           | rpp::ops::with_latest_from([](const auto&, const auto& v) { return v; }, drawable_objects)
           | rpp::ops::switch_on_next();
}