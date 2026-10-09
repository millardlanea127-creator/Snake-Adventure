

#include <SFML/Graphics.hpp>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <queue>
#include <random>
#include <string>
#include <vector>

using namespace std;

// ================= 基础参数 =================

constexpr int CELL = 25;
constexpr int COLS = 24;
constexpr int ROWS = 20;

constexpr int GAME_W = COLS * CELL;
constexpr int GAME_H = ROWS * CELL;

constexpr int PANEL_W = 260;
constexpr int WINDOW_W = GAME_W + PANEL_W;
constexpr int WINDOW_H = GAME_H;

constexpr int MAX_LEVEL = 5;
constexpr int WIN_SCORE = 250;

constexpr float PI = 3.14159265358979323846f;

// ================= 数据结构 =================

struct Point
{
    int x, y;

    bool operator==(const Point& other) const
    {
        return x == other.x && y == other.y;
    }
};

enum class Scene
{
    MainMenu,
    BackgroundMenu,
    SnakeMenu,
    Playing,
    Paused,
    Dead,
    Win
};

enum class HeadStyle
{
    Bunny,
    Bear,
    Penguin
};

struct Picker
{
    float hue = 140.f;
    float saturation = 0.8f;
};

// ================= 颜色转换 =================

sf::Color hsv(float h, float s, float v = 1.f)
{
    h = fmod(h + 360.f, 360.f);

    float c = v * s;
    float x = c * (1.f - fabs(fmod(h / 60.f, 2.f) - 1.f));
    float m = v - c;

    float r = 0.f;
    float g = 0.f;
    float b = 0.f;

    if (h < 60.f)
    {
        r = c;
        g = x;
    }
    else if (h < 120.f)
    {
        r = x;
        g = c;
    }
    else if (h < 180.f)
    {
        g = c;
        b = x;
    }
    else if (h < 240.f)
    {
        g = x;
        b = c;
    }
    else if (h < 300.f)
    {
        r = x;
        b = c;
    }
    else
    {
        r = c;
        b = x;
    }

    return sf::Color(
        uint8_t((r + m) * 255.f),
        uint8_t((g + m) * 255.f),
        uint8_t((b + m) * 255.f)
    );
}

sf::Color pickerColor(const Picker& picker)
{
    return hsv(picker.hue, picker.saturation);
}

// ================= 选择图片 =================

wstring chooseImageFile()
{
    wchar_t filename[4096] = {};

    OPENFILENAMEW dialog{};

    dialog.lStructSize = sizeof(dialog);
    dialog.lpstrFile = filename;
    dialog.nMaxFile = 4096;

    dialog.lpstrFilter =
        L"图片文件\0*.png;*.jpg;*.jpeg;*.bmp\0"
        L"所有文件\0*.*\0";

    dialog.Flags =
        OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (GetOpenFileNameW(&dialog))
        return filename;

    return L"";
}

// ================= 主程序 =================

int main()
{
    sf::RenderWindow window(
        sf::VideoMode({WINDOW_W, WINDOW_H}),
        "Snake Adventure"
    );

    window.setFramerateLimit(60);
    window.setKeyRepeatEnabled(false);

    // ================= 中文字体 =================

    sf::Font font;
    bool fontLoaded = false;

    for (const char* path : {
        "C:/Windows/Fonts/msyh.ttc",
        "C:/Windows/Fonts/msyhbd.ttc",
        "C:/Windows/Fonts/simhei.ttf",
        "C:/Windows/Fonts/simsun.ttc"
    })
    {
        if (font.openFromFile(path))
        {
            fontLoaded = true;
            break;
        }
    }

    if (!fontLoaded)
    {
        cerr << "无法加载中文字体" << endl;
        return 1;
    }

    // ================= 随机数 =================

    mt19937 rng(random_device{}());

    auto randInt = [&](int left, int right)
    {
        return uniform_int_distribution<int>(
            left, right
        )(rng);
    };

    // ================= 游戏设置 =================

    Picker backgroundPicker{215.f, 0.25f};
    Picker snakePicker{140.f, 0.75f};

    HeadStyle headStyle = HeadStyle::Bunny;

    sf::Texture backgroundTexture;

    bool hasBackgroundImage = false;
    int imageOpacity = 160;

    Scene scene = Scene::MainMenu;

    vector<Point> snake;

    Point food{-1, -1};
    Point monster{-1, -1};

    int direction = 0;
    int nextDirection = 0;

    int score = 0;
    int level = 1;

    int patrolDirection = 0;
    int patrolSteps = 0;

    bool directionChanged = false;

    float snakeTimer = 0.f;
    float monsterTimer = 0.f;
    float moveInterval = 0.30f;

    sf::Clock clock;

    // 0右 1下 2左 3上
    constexpr array<Point, 4> DIRS = {
        Point{1, 0},
        Point{0, 1},
        Point{-1, 0},
        Point{0, -1}
    };

    const sf::Color white(245, 247, 250);
    const sf::Color dark(35, 43, 55);
    const sf::Color gray(160, 174, 190);
    const sf::Color purple(188, 90, 240);
    const sf::Color red(255, 85, 95);

    // ================= 绘图工具 =================

    auto drawRect = [&](float x, float y,
                        float w, float h,
                        sf::Color color)
    {
        sf::RectangleShape shape({w, h});
        shape.setPosition({x, y});
        shape.setFillColor(color);
        window.draw(shape);
    };

    auto drawText = [&](const wstring& content,
                        float x, float y,
                        unsigned int size,
                        sf::Color color)
    {
        sf::Text text(
            font,
            sf::String(content),
            size
        );

        text.setPosition({x, y});
        text.setFillColor(color);

        window.draw(text);
    };

    auto inside = [&](float mx, float my,
                      float x, float y,
                      float w, float h)
    {
        return mx >= x && mx <= x + w &&
               my >= y && my <= y + h;
    };

    auto drawButton = [&](const wstring& content,
                          float x, float y,
                          float w, float h,
                          sf::Color color)
    {
        sf::Vector2i mouse =
            sf::Mouse::getPosition(window);

        if (inside(float(mouse.x),
                   float(mouse.y),
                   x, y, w, h))
        {
            color.r = uint8_t(
                min(255, int(color.r) + 20)
            );

            color.g = uint8_t(
                min(255, int(color.g) + 20)
            );

            color.b = uint8_t(
                min(255, int(color.b) + 20)
            );
        }

        sf::RectangleShape shape({w, h});

        shape.setPosition({x, y});
        shape.setFillColor(color);
        shape.setOutlineThickness(1.f);
        shape.setOutlineColor(
            sf::Color(130, 145, 160)
        );

        window.draw(shape);

        sf::Text text(
            font,
            sf::String(content),
            17
        );

        text.setFillColor(white);

        auto bounds = text.getLocalBounds();

        text.setPosition({
            x + (w - bounds.size.x) / 2.f
              - bounds.position.x,
            y + (h - bounds.size.y) / 2.f
              - bounds.position.y
        });

        window.draw(text);
    };

    // ================= 可爱眼睛 =================

    auto drawEyes = [&](float x, float y)
    {
        sf::CircleShape eye(2.6f);
        eye.setFillColor(sf::Color::White);

        sf::CircleShape pupil(1.2f);
        pupil.setFillColor(sf::Color::Black);

        eye.setPosition({x + 6.f, y + 7.f});
        pupil.setPosition({x + 7.f, y + 8.f});

        window.draw(eye);
        window.draw(pupil);

        eye.setPosition({x + 15.f, y + 7.f});
        pupil.setPosition({x + 16.f, y + 8.f});

        window.draw(eye);
        window.draw(pupil);
    };

    // ================= 绘制蛇头 =================

    auto drawHead = [&](float x, float y,
                        HeadStyle style,
                        sf::Color color)
    {
        if (style == HeadStyle::Bunny)
        {
            // 兔耳朵
            sf::CircleShape ear(5.f);
            ear.setScale({0.7f, 1.8f});
            ear.setFillColor(color);

            ear.setPosition({x + 4.f, y - 6.f});
            window.draw(ear);

            ear.setPosition({x + 13.f, y - 6.f});
            window.draw(ear);

            // 脸
            sf::CircleShape face(10.f);
            face.setFillColor(color);
            face.setPosition({x + 2.f, y + 4.f});
            window.draw(face);

            drawEyes(x + 2.f, y + 4.f);

            sf::CircleShape nose(1.8f);
            nose.setFillColor(
                sf::Color(255, 160, 185)
            );
            nose.setPosition({x + 11.f, y + 17.f});
            window.draw(nose);
        }
        else if (style == HeadStyle::Bear)
        {
            sf::CircleShape ear(5.f);
            ear.setFillColor(color);

            ear.setPosition({x + 2.f, y + 2.f});
            window.draw(ear);

            ear.setPosition({x + 15.f, y + 2.f});
            window.draw(ear);

            sf::CircleShape face(11.f);
            face.setFillColor(color);
            face.setPosition({x + 1.f, y + 4.f});
            window.draw(face);

            drawEyes(x + 1.f, y + 4.f);

            sf::CircleShape muzzle(3.5f);
            muzzle.setFillColor(
                sf::Color(245, 225, 210)
            );
            muzzle.setPosition({x + 9.f, y + 16.f});
            window.draw(muzzle);
        }
        else
        {
            sf::CircleShape face(11.f);
            face.setFillColor(color);
            face.setPosition({x + 1.f, y + 3.f});
            window.draw(face);

            sf::CircleShape belly(8.f);
            belly.setFillColor(
                sf::Color(245, 245, 245)
            );
            belly.setPosition({x + 4.f, y + 8.f});
            window.draw(belly);

            drawEyes(x + 1.f, y + 4.f);

            sf::ConvexShape beak;
            beak.setPointCount(3);

            beak.setPoint(0, {x + 8.f, y + 15.f});
            beak.setPoint(1, {x + 16.f, y + 15.f});
            beak.setPoint(2, {x + 12.f, y + 19.f});

            beak.setFillColor(
                sf::Color(255, 180, 45)
            );

            window.draw(beak);
        }
    };

    // ================= 绘制怪兽 =================

    auto drawMonster = [&](float x, float y)
    {
        sf::CircleShape shape(11.f);
        shape.setPosition({x, y});
        shape.setFillColor(purple);

        window.draw(shape);
        drawEyes(x, y);
    };

    // ================= 主菜单图标 =================

    auto drawMenuIcon = [&](float cx, float cy,
                            float radius,
                            int type,
                            const wstring& label,
                            sf::Color color)
    {
        sf::Vector2i mouse =
            sf::Mouse::getPosition(window);

        float dx = mouse.x - cx;
        float dy = mouse.y - cy;

        bool hovered =
            dx * dx + dy * dy <= radius * radius;

        sf::CircleShape circle(radius);

        circle.setOrigin({radius, radius});
        circle.setPosition({cx, cy});

        circle.setFillColor(color);

        circle.setOutlineThickness(
            hovered ? 5.f : 2.f
        );

        circle.setOutlineColor(
            sf::Color::White
        );

        window.draw(circle);

        if (type == 0)
        {
            // 背景图标：图片框
            sf::RectangleShape frame({50.f, 35.f});

            frame.setOrigin({25.f, 17.5f});
            frame.setPosition({cx, cy});
            frame.setFillColor(
                sf::Color(255, 255, 255, 60)
            );
            frame.setOutlineThickness(2.f);
            frame.setOutlineColor(white);

            window.draw(frame);

            sf::CircleShape sun(6.f);
            sun.setFillColor(
                sf::Color(255, 225, 80)
            );
            sun.setPosition({
                cx + 9.f, cy - 12.f
            });

            window.draw(sun);
        }
        else if (type == 1)
        {
            // 小蛇图标
            sf::CircleShape piece(9.f);
            piece.setFillColor(
                sf::Color(70, 230, 130)
            );

            piece.setPosition({
                cx - 27.f, cy + 7.f
            });
            window.draw(piece);

            piece.setPosition({
                cx - 11.f, cy + 3.f
            });
            window.draw(piece);

            piece.setPosition({
                cx + 5.f, cy - 3.f
            });
            window.draw(piece);

            drawHead(
                cx + 17.f, cy - 15.f,
                HeadStyle::Bear,
                sf::Color(95, 255, 160)
            );
        }
        else
        {
            // 播放图标
            sf::ConvexShape triangle;

            triangle.setPointCount(3);

            triangle.setPoint(0, {
                cx - 13.f, cy - 19.f
            });

            triangle.setPoint(1, {
                cx - 13.f, cy + 19.f
            });

            triangle.setPoint(2, {
                cx + 20.f, cy
            });

            triangle.setFillColor(white);

            window.draw(triangle);
        }

        sf::Text text(
            font,
            sf::String(label),
            21
        );

        text.setFillColor(
            sf::Color(40, 48, 60)
        );

        auto bounds = text.getLocalBounds();

        text.setPosition({
            cx - bounds.size.x / 2.f
               - bounds.position.x,
            cy + radius + 19.f
        });

        window.draw(text);
    };

    // ================= 色轮设置 =================

    constexpr float WHEEL_X = 260.f;
    constexpr float WHEEL_Y = 260.f;

    constexpr float WHEEL_INNER = 45.f;
    constexpr float WHEEL_OUTER = 165.f;

    constexpr int SECTORS = 12;
    constexpr int RINGS = 6;

    // SFML 3 顶点辅助函数
    auto vertex = [&](sf::Vector2f position,
                      sf::Color color)
    {
        sf::Vertex v;
        v.position = position;
        v.color = color;
        return v;
    };

    // ================= 绘制色轮 =================

    auto drawColorWheel = [&](const Picker& picker)
    {
        sf::VertexArray wheel(
            sf::PrimitiveType::Triangles
        );

        for (int ring = 0; ring < RINGS; ring++)
        {
            float r0 = WHEEL_INNER +
                (WHEEL_OUTER - WHEEL_INNER) *
                float(ring) / RINGS;

            float r1 = WHEEL_INNER +
                (WHEEL_OUTER - WHEEL_INNER) *
                float(ring + 1) / RINGS;

            for (int sector = 0;
                 sector < SECTORS;
                 sector++)
            {
                float a0 =
                    -PI / 2.f +
                    2.f * PI * sector / SECTORS;

                float a1 =
                    -PI / 2.f +
                    2.f * PI * (sector + 1) / SECTORS;

                float hue =
                    60.f + 360.f * sector / SECTORS;

                hue = fmod(hue, 360.f);

                float s0 = float(ring) / RINGS;
                float s1 = float(ring + 1) / RINGS;

                sf::Color c0 = hsv(hue, s0);
                sf::Color c1 = hsv(hue, s1);

                sf::Vector2f p1{
                    WHEEL_X + r0 * cos(a0),
                    WHEEL_Y + r0 * sin(a0)
                };

                sf::Vector2f p2{
                    WHEEL_X + r1 * cos(a0),
                    WHEEL_Y + r1 * sin(a0)
                };

                sf::Vector2f p3{
                    WHEEL_X + r1 * cos(a1),
                    WHEEL_Y + r1 * sin(a1)
                };

                sf::Vector2f p4{
                    WHEEL_X + r0 * cos(a1),
                    WHEEL_Y + r0 * sin(a1)
                };

                wheel.append(vertex(p1, c0));
                wheel.append(vertex(p2, c1));
                wheel.append(vertex(p3, c1));

                wheel.append(vertex(p1, c0));
                wheel.append(vertex(p3, c1));
                wheel.append(vertex(p4, c0));
            }
        }

        window.draw(wheel);

        // 中间白色圆
        sf::CircleShape center(WHEEL_INNER);

        center.setOrigin({
            WHEEL_INNER,
            WHEEL_INNER
        });

        center.setPosition({
            WHEEL_X,
            WHEEL_Y
        });

        center.setFillColor(
            sf::Color(250, 250, 250)
        );

        window.draw(center);

        // 放射分割线
        for (int i = 0; i < SECTORS; i++)
        {
            float a =
                -PI / 2.f +
                2.f * PI * i / SECTORS;

            sf::Vertex line[2];

            line[0].position = {
                WHEEL_X + WHEEL_INNER * cos(a),
                WHEEL_Y + WHEEL_INNER * sin(a)
            };

            line[1].position = {
                WHEEL_X + WHEEL_OUTER * cos(a),
                WHEEL_Y + WHEEL_OUTER * sin(a)
            };

            line[0].color =
                sf::Color(255, 255, 255, 170);

            line[1].color =
                sf::Color(255, 255, 255, 170);

            window.draw(
                line, 2,
                sf::PrimitiveType::Lines
            );
        }

        // 同心圆分割线
        for (int i = 1; i <= RINGS; i++)
        {
            float r =
                WHEEL_INNER +
                (WHEEL_OUTER - WHEEL_INNER) *
                float(i) / RINGS;

            sf::CircleShape ring(r);

            ring.setOrigin({r, r});
            ring.setPosition({
                WHEEL_X, WHEEL_Y
            });

            ring.setFillColor(
                sf::Color::Transparent
            );

            ring.setOutlineThickness(1.f);

            ring.setOutlineColor(
                sf::Color(255, 255, 255, 140)
            );

            window.draw(ring);
        }

        // 选中的颜色位置
        float r =
            WHEEL_INNER +
            picker.saturation *
            (WHEEL_OUTER - WHEEL_INNER);

        float a =
            -PI / 2.f +
            picker.hue / 360.f * 2.f * PI;

        float x = WHEEL_X + r * cos(a);
        float y = WHEEL_Y + r * sin(a);

        sf::CircleShape marker(7.f);

        marker.setOrigin({7.f, 7.f});
        marker.setPosition({x, y});

        marker.setFillColor(
            sf::Color::White
        );

        marker.setOutlineThickness(2.f);
        marker.setOutlineColor(
            sf::Color::Black
        );

        window.draw(marker);
    };

    // ================= 选择色轮颜色 =================

    auto pickWheel = [&](float x, float y,
                         Picker& picker)
    {
        float dx = x - WHEEL_X;
        float dy = y - WHEEL_Y;

        float radius = sqrt(dx * dx + dy * dy);

        if (radius < WHEEL_INNER ||
            radius > WHEEL_OUTER)
        {
            return false;
        }

        float angle =
            atan2(dy, dx) * 180.f / PI + 90.f;

        if (angle < 0.f)
            angle += 360.f;

        picker.hue = angle;

        picker.saturation = clamp(
            (radius - WHEEL_INNER) /
            (WHEEL_OUTER - WHEEL_INNER),
            0.f, 1.f
        );

        return true;
    };

    // ================= 实时背景预览 =================

    auto drawPreview = [&](float x, float y,
                           float w, float h)
    {
        // 边框
        drawRect(
            x - 3.f, y - 3.f,
            w + 6.f, h + 6.f,
            sf::Color(80, 95, 110)
        );

        // 当前背景颜色
        drawRect(
            x, y, w, h,
            pickerColor(backgroundPicker)
        );

        // 图片
        if (hasBackgroundImage)
        {
            sf::Sprite sprite(backgroundTexture);

            auto size =
                backgroundTexture.getSize();

            if (size.x > 0 && size.y > 0)
            {
                sprite.setScale({
                    w / float(size.x),
                    h / float(size.y)
                });

                sprite.setPosition({x, y});

                sprite.setColor(
                    sf::Color(
                        255, 255, 255,
                        uint8_t(imageOpacity)
                    )
                );

                window.draw(sprite);
            }
        }

        // 网格
        for (float gx = x; gx < x + w; gx += 20.f)
        {
            drawRect(
                gx, y, 1.f, h,
                sf::Color(255, 255, 255, 45)
            );
        }

        for (float gy = y; gy < y + h; gy += 20.f)
        {
            drawRect(
                x, gy, w, 1.f,
                sf::Color(255, 255, 255, 45)
            );
        }

        // 示例小蛇
        sf::Color color = pickerColor(snakePicker);

        drawRect(
            x + 22.f,
            y + h - 42.f,
            23.f, 23.f,
            color
        );

        drawRect(
            x + 47.f,
            y + h - 42.f,
            23.f, 23.f,
            color
        );

        drawRect(
            x + 72.f,
            y + h - 42.f,
            23.f, 23.f,
            color
        );

        drawHead(
            x + 98.f,
            y + h - 45.f,
            headStyle,
            color
        );

        // 示例怪兽
        drawMonster(
            x + w - 43.f,
            y + 23.f
        );

        // 示例食物
        sf::CircleShape foodShape(8.f);

        foodShape.setFillColor(red);

        foodShape.setPosition({
            x + w - 42.f,
            y + h - 35.f
        });

        window.draw(foodShape);
    };

    // ================= 蛇碰撞工具 =================

    auto valid = [&](Point p)
    {
        return p.x >= 0 && p.x < COLS &&
               p.y >= 0 && p.y < ROWS;
    };

    auto onSnake = [&](Point p)
    {
        return find(
            snake.begin(),
            snake.end(),
            p
        ) != snake.end();
    };

    // ================= 随机生成空位置 =================

    auto randomFree = [&](bool forFood)
    {
        vector<Point> available;

        for (int y = 0; y < ROWS; y++)
        {
            for (int x = 0; x < COLS; x++)
            {
                Point p{x, y};

                if (onSnake(p))
                    continue;

                if (forFood && p == monster)
                    continue;

                if (!forFood && p == food)
                    continue;

                available.push_back(p);
            }
        }

        if (available.empty())
            return Point{-1, -1};

        return available[
            randInt(
                0,
                int(available.size()) - 1
            )
        ];
    };

    auto spawnFood = [&]()
    {
        food = randomFree(true);

        if (food.x == -1)
            scene = Scene::Win;
    };

    auto spawnMonster = [&]()
    {
        monster = randomFree(false);
    };

    // ================= 重新开始 =================

    auto resetGame = [&]()
    {
        snake = {
            {5, 10},
            {4, 10},
            {3, 10}
        };

        direction = 0;
        nextDirection = 0;

        directionChanged = false;

        score = 0;
        level = 1;

        moveInterval = 0.30f;

        snakeTimer = 0.f;
        monsterTimer = 0.f;

        patrolSteps = 0;
        patrolDirection = 0;

        food = {-1, -1};
        monster = {-1, -1};

        scene = Scene::Playing;

        spawnFood();

        if (scene == Scene::Playing)
            spawnMonster();

        clock.restart();
    };

    // ================= BFS 追踪 =================

    auto bfsStep = [&]() -> Point
    {
        if (monster.x < 0 || snake.empty())
            return monster;

        Point start = monster;
        Point goal = snake.front();

        array<array<bool, ROWS>, COLS> visited{};
        array<array<Point, ROWS>, COLS> previous{};

        queue<Point> q;

        q.push(start);
        visited[start.x][start.y] = true;

        while (!q.empty())
        {
            Point p = q.front();
            q.pop();

            if (p == goal)
                break;

            for (Point d : DIRS)
            {
                Point next{
                    p.x + d.x,
                    p.y + d.y
                };

                if (!valid(next))
                    continue;

                if (visited[next.x][next.y])
                    continue;

                if (onSnake(next) && !(next == goal))
                    continue;

                // 怪兽不进入食物格
                if (next == food)
                    continue;

                visited[next.x][next.y] = true;
                previous[next.x][next.y] = p;

                q.push(next);
            }
        }

        if (!visited[goal.x][goal.y])
            return start;

        Point p = goal;

        while (!(previous[p.x][p.y] == start))
        {
            p = previous[p.x][p.y];
        }

        return p;
    };

    // ================= 怪兽移动 =================

    auto moveMonster = [&]()
    {
        if (monster.x < 0 || snake.empty())
            return;

        bool chase = false;

        if (level == 3)
            chase = randInt(0, 99) < 35;

        if (level == 4)
            chase = randInt(0, 99) < 60;

        if (level == 5)
            chase = true;

        Point target = monster;

        if (chase)
            target = bfsStep();

        if (target == monster)
        {
            // 连续巡逻3~6格
            if (patrolSteps <= 0)
            {
                patrolDirection = randInt(0, 3);
                patrolSteps = randInt(3, 6);
            }

            for (int i = 0; i < 4; i++)
            {
                Point d = DIRS[patrolDirection];

                Point next{
                    monster.x + d.x,
                    monster.y + d.y
                };

                if (valid(next) && !(next == food))
                {
                    target = next;
                    patrolSteps--;
                    break;
                }

                patrolDirection =
                    (patrolDirection + 1) % 4;

                patrolSteps = randInt(3, 6);
            }
        }
        else
        {
            patrolSteps = 0;
        }

        monster = target;

        // 只有怪兽碰到蛇头才死亡
        if (monster == snake.front())
        {
            scene = Scene::Dead;
        }
    };

    // ================= 蛇移动 =================

    auto moveSnake = [&]()
    {
        direction = nextDirection;
        directionChanged = false;

        Point d = DIRS[direction];

        Point newHead{
            snake.front().x + d.x,
            snake.front().y + d.y
        };

        // 撞墙
        if (!valid(newHead))
        {
            scene = Scene::Dead;
            return;
        }

        bool eating = newHead == food;

        // 撞自己
        size_t count = snake.size();

        if (!eating)
            count--;

        for (size_t i = 0; i < count; i++)
        {
            if (snake[i] == newHead)
            {
                scene = Scene::Dead;
                return;
            }
        }

        // 蛇头撞怪兽
        if (newHead == monster)
        {
            scene = Scene::Dead;
            return;
        }

        snake.insert(
            snake.begin(),
            newHead
        );

        if (eating)
        {
            score += 10;

            if (score >= WIN_SCORE)
            {
                score = WIN_SCORE;
                level = MAX_LEVEL;
                scene = Scene::Win;
                return;
            }

            level = min(
                MAX_LEVEL,
                score / 50 + 1
            );

            moveInterval =
                0.30f - 0.05f * (level - 1);

            spawnFood();
        }
        else
        {
            snake.pop_back();
        }
    };

    // ================= 游戏背景 =================

    auto drawGameBackground = [&]()
    {
        drawRect(
            0, 0,
            float(GAME_W),
            float(GAME_H),
            pickerColor(backgroundPicker)
        );

        if (hasBackgroundImage)
        {
            sf::Sprite sprite(backgroundTexture);

            auto size =
                backgroundTexture.getSize();

            sprite.setScale({
                float(GAME_W) / size.x,
                float(GAME_H) / size.y
            });

            sprite.setColor(
                sf::Color(
                    255, 255, 255,
                    uint8_t(imageOpacity)
                )
            );

            window.draw(sprite);
        }

        for (int x = 0; x < GAME_W; x += CELL)
        {
            drawRect(
                float(x), 0,
                1.f, float(GAME_H),
                sf::Color(255, 255, 255, 40)
            );
        }

        for (int y = 0; y < GAME_H; y += CELL)
        {
            drawRect(
                0, float(y),
                float(GAME_W), 1.f,
                sf::Color(255, 255, 255, 40)
            );
        }
    };

    // ================= 菜单交互状态 =================

    bool draggingWheel = false;
    bool draggingOpacity = false;

    // 背景设置布局
    constexpr float PREVIEW_X = 490.f;
    constexpr float PREVIEW_Y = 140.f;
    constexpr float PREVIEW_W = 325.f;
    constexpr float PREVIEW_H = 155.f;

    constexpr float OPACITY_X = 520.f;
    constexpr float OPACITY_Y = 378.f;
    constexpr float OPACITY_W = 265.f;

    // ================= 主循环 =================

    while (window.isOpen())
    {
        float dt = min(
            clock.restart().asSeconds(),
            0.25f
        );

        // ================= 处理事件 =================

        while (const auto event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            // ---------- 键盘 ----------
            if (const auto* key =
                event->getIf<sf::Event::KeyPressed>())
            {
                auto code = key->code;

                if (code == sf::Keyboard::Key::Escape)
                {
                    if (scene == Scene::MainMenu)
                        window.close();
                    else
                        scene = Scene::MainMenu;

                    draggingWheel = false;
                    draggingOpacity = false;

                    continue;
                }

                if (scene != Scene::MainMenu &&
                    scene != Scene::BackgroundMenu &&
                    scene != Scene::SnakeMenu)
                {
                    if (code == sf::Keyboard::Key::R)
                    {
                        resetGame();
                        continue;
                    }

                    if (code == sf::Keyboard::Key::Space)
                    {
                        if (scene == Scene::Playing)
                        {
                            scene = Scene::Paused;
                        }
                        else if (scene == Scene::Paused)
                        {
                            scene = Scene::Playing;
                            snakeTimer = 0.f;
                            monsterTimer = 0.f;
                        }

                        continue;
                    }
                }

                if (scene == Scene::Playing &&
                    !directionChanged)
                {
                    int wanted = -1;

                    if (code == sf::Keyboard::Key::D ||
                        code == sf::Keyboard::Key::Right)
                        wanted = 0;

                    if (code == sf::Keyboard::Key::S ||
                        code == sf::Keyboard::Key::Down)
                        wanted = 1;

                    if (code == sf::Keyboard::Key::A ||
                        code == sf::Keyboard::Key::Left)
                        wanted = 2;

                    if (code == sf::Keyboard::Key::W ||
                        code == sf::Keyboard::Key::Up)
                        wanted = 3;

                    // 防止直接反向
                    if (wanted != -1 &&
                        (wanted + 2) % 4 != direction)
                    {
                        nextDirection = wanted;
                        directionChanged = true;
                    }
                }
            }

            // ---------- 鼠标按下 ----------
            if (const auto* mouse =
                event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouse->button !=
                    sf::Mouse::Button::Left)
                    continue;

                float mx = float(mouse->position.x);
                float my = float(mouse->position.y);

                if (scene == Scene::MainMenu)
                {
                    auto clickCircle =
                        [&](float cx, float cy, float r)
                    {
                        float dx = mx - cx;
                        float dy = my - cy;

                        return dx * dx + dy * dy <= r * r;
                    };

                    if (clickCircle(160.f, 260.f, 72.f))
                        scene = Scene::BackgroundMenu;

                    else if (clickCircle(430.f, 260.f, 72.f))
                        scene = Scene::SnakeMenu;

                    else if (clickCircle(700.f, 260.f, 72.f))
                        resetGame();
                }

                else if (scene == Scene::BackgroundMenu)
                {
                    if (pickWheel(
                        mx, my, backgroundPicker))
                    {
                        draggingWheel = true;
                    }

                    else if (inside(
                        mx, my, 510.f, 320.f, 145.f, 40.f))
                    {
                        wstring path = chooseImageFile();

                        if (!path.empty())
                        {
                            if (backgroundTexture.loadFromFile(
                                filesystem::path(path)))
                            {
                                hasBackgroundImage = true;
                            }
                        }

                        clock.restart();
                    }

                    else if (inside(
                        mx, my, 665.f, 320.f, 145.f, 40.f))
                    {
                        hasBackgroundImage = false;
                    }

                    else if (inside(
                        mx, my,
                        OPACITY_X, OPACITY_Y - 9.f,
                        OPACITY_W, 26.f))
                    {
                        draggingOpacity = true;

                        imageOpacity = clamp(
                            int((mx - OPACITY_X) /
                                OPACITY_W * 255.f),
                            0, 255
                        );
                    }

                    else if (inside(
                        mx, my, 580.f, 433.f, 160.f, 44.f))
                    {
                        scene = Scene::MainMenu;
                    }
                }

                else if (scene == Scene::SnakeMenu)
                {
                    if (pickWheel(mx, my, snakePicker))
                    {
                        draggingWheel = true;
                    }

                    else if (inside(
                        mx, my, 555.f, 115.f, 220.f, 43.f))
                    {
                        headStyle = HeadStyle::Bunny;
                    }

                    else if (inside(
                        mx, my, 555.f, 168.f, 220.f, 43.f))
                    {
                        headStyle = HeadStyle::Bear;
                    }

                    else if (inside(
                        mx, my, 555.f, 221.f, 220.f, 43.f))
                    {
                        headStyle = HeadStyle::Penguin;
                    }

                    else if (inside(
                        mx, my, 580.f, 433.f, 160.f, 44.f))
                    {
                        scene = Scene::MainMenu;
                    }
                }
            }

            // ---------- 鼠标移动 ----------
            if (const auto* mouse =
                event->getIf<sf::Event::MouseMoved>())
            {
                float mx = float(mouse->position.x);
                float my = float(mouse->position.y);

                if (draggingWheel)
                {
                    if (scene == Scene::BackgroundMenu)
                        pickWheel(
                            mx, my, backgroundPicker
                        );

                    else if (scene == Scene::SnakeMenu)
                        pickWheel(
                            mx, my, snakePicker
                        );
                }

                if (draggingOpacity &&
                    scene == Scene::BackgroundMenu)
                {
                    imageOpacity = clamp(
                        int((mx - OPACITY_X) /
                            OPACITY_W * 255.f),
                        0, 255
                    );
                }
            }

            // ---------- 鼠标松开 ----------
            if (const auto* mouse =
                event->getIf<sf::Event::MouseButtonReleased>())
            {
                if (mouse->button ==
                    sf::Mouse::Button::Left)
                {
                    draggingWheel = false;
                    draggingOpacity = false;
                }
            }
        }

        // ================= 游戏更新 =================

        if (scene == Scene::Playing)
        {
            snakeTimer += dt;
            monsterTimer += dt;

            float monsterInterval;

            if (level <= 2)
                monsterInterval = 0.42f;
            else if (level == 3)
                monsterInterval = 0.34f;
            else if (level == 4)
                monsterInterval = 0.28f;
            else
                monsterInterval = 0.23f;

            if (monsterTimer >= monsterInterval)
            {
                monsterTimer = 0.f;
                moveMonster();
            }

            if (scene == Scene::Playing &&
                snakeTimer >= moveInterval)
            {
                snakeTimer = 0.f;
                moveSnake();
            }
        }

        // ================= 开始绘图 =================

        window.clear(
            sf::Color(238, 241, 245)
        );

        // ================= 主菜单 =================

        if (scene == Scene::MainMenu)
        {
            drawText(
                L"贪 吃 蛇 大 冒 险",
                260.f, 72.f, 37,
                sf::Color(35, 42, 50)
            );

            drawText(
                L"选择一个功能，开启你的冒险",
                287.f, 132.f, 19,
                sf::Color(100, 110, 120)
            );

            drawMenuIcon(
                160.f, 260.f, 72.f,
                0, L"修改背景",
                sf::Color(150, 205, 245)
            );

            drawMenuIcon(
                430.f, 260.f, 72.f,
                1, L"修改小蛇",
                sf::Color(160, 230, 180)
            );

            drawMenuIcon(
                700.f, 260.f, 72.f,
                2, L"开始游戏",
                sf::Color(245, 200, 125)
            );

            drawText(
                L"共五关 · 自定义造型 · 智能怪兽",
                280.f, 428.f, 18,
                sf::Color(110, 120, 130)
            );
        }

        // ================= 背景设置 =================

        else if (scene == Scene::BackgroundMenu)
        {
            drawText(
                L"背景设置",
                340.f, 18.f, 31,
                sf::Color(35, 42, 50)
            );

            drawText(
                L"点击色轮选择颜色",
                185.f, 70.f, 18,
                sf::Color(90, 100, 110)
            );

            drawColorWheel(backgroundPicker);

            drawText(
                L"实时预览",
                595.f, 92.f, 22,
                sf::Color(45, 55, 65)
            );

            // 背景实时预览
            drawPreview(
                PREVIEW_X,
                PREVIEW_Y,
                PREVIEW_W,
                PREVIEW_H
            );

            drawButton(
                L"选择背景图片",
                510.f, 320.f,
                145.f, 40.f,
                sf::Color(65, 130, 185)
            );

            drawButton(
                L"清除图片",
                665.f, 320.f,
                145.f, 40.f,
                sf::Color(145, 95, 115)
            );

            drawText(
                L"图片透明度",
                520.f, 351.f, 15,
                sf::Color(80, 90, 100)
            );

            // 透明度滑块
            drawRect(
                OPACITY_X, OPACITY_Y,
                OPACITY_W, 10.f,
                sf::Color(175, 185, 195)
            );

            drawRect(
                OPACITY_X, OPACITY_Y,
                OPACITY_W * imageOpacity / 255.f,
                10.f,
                sf::Color(65, 160, 245)
            );

            drawText(
                to_wstring(
                    int(round(imageOpacity / 255.f * 100))
                ) + L"%",
                790.f, 369.f, 15,
                sf::Color(55, 65, 75)
            );

            drawButton(
                L"返回主菜单",
                580.f, 433.f,
                160.f, 44.f,
                sf::Color(70, 150, 105)
            );
        }

        // ================= 小蛇设置 =================

        else if (scene == Scene::SnakeMenu)
        {
            drawText(
                L"小蛇设置",
                340.f, 18.f, 31,
                sf::Color(35, 42, 50)
            );

            drawText(
                L"点击色轮选择小蛇颜色",
                160.f, 70.f, 18,
                sf::Color(90, 100, 110)
            );

            drawColorWheel(snakePicker);

            drawText(
                L"选择蛇头造型",
                585.f, 72.f, 21,
                sf::Color(45, 55, 65)
            );

            drawButton(
                L"小兔子",
                555.f, 115.f,
                220.f, 43.f,
                headStyle == HeadStyle::Bunny
                    ? sf::Color(65, 160, 110)
                    : sf::Color(110, 125, 140)
            );

            drawButton(
                L"小熊",
                555.f, 168.f,
                220.f, 43.f,
                headStyle == HeadStyle::Bear
                    ? sf::Color(65, 160, 110)
                    : sf::Color(110, 125, 140)
            );

            drawButton(
                L"小企鹅",
                555.f, 221.f,
                220.f, 43.f,
                headStyle == HeadStyle::Penguin
                    ? sf::Color(65, 160, 110)
                    : sf::Color(110, 125, 140)
            );

            drawText(
                L"当前小蛇预览",
                585.f, 285.f, 18,
                sf::Color(70, 80, 90)
            );

            drawRect(
                555.f, 320.f,
                220.f, 75.f,
                sf::Color(215, 225, 232)
            );

            sf::Color color =
                pickerColor(snakePicker);

            drawRect(
                595.f, 344.f,
                23.f, 23.f, color
            );

            drawRect(
                620.f, 344.f,
                23.f, 23.f, color
            );

            drawRect(
                645.f, 344.f,
                23.f, 23.f, color
            );

            drawHead(
                672.f, 341.f,
                headStyle, color
            );

            drawButton(
                L"返回主菜单",
                580.f, 433.f,
                160.f, 44.f,
                sf::Color(70, 150, 105)
            );
        }

        // ================= 游戏界面 =================

        else
        {
            drawGameBackground();

            // 食物
            if (food.x >= 0 &&
                scene != Scene::Win)
            {
                sf::CircleShape foodShape(9.f);

                foodShape.setFillColor(red);

                foodShape.setPosition({
                    float(food.x * CELL + 3),
                    float(food.y * CELL + 3)
                });

                window.draw(foodShape);
            }

            // 蛇身
            sf::Color snakeColor =
                pickerColor(snakePicker);

            for (size_t i = 1; i < snake.size(); i++)
            {
                drawRect(
                    float(snake[i].x * CELL + 1),
                    float(snake[i].y * CELL + 1),
                    CELL - 2.f,
                    CELL - 2.f,
                    snakeColor
                );
            }

            // 蛇头
            if (!snake.empty())
            {
                drawHead(
                    float(snake.front().x * CELL + 1),
                    float(snake.front().y * CELL + 1),
                    headStyle,
                    snakeColor
                );
            }

            // 怪兽
            if (monster.x >= 0)
            {
                drawMonster(
                    float(monster.x * CELL + 1),
                    float(monster.y * CELL + 1)
                );
            }

            // ================= 右侧信息栏 =================

            drawRect(
                float(GAME_W), 0.f,
                float(PANEL_W), float(GAME_H),
                dark
            );

            float px = GAME_W + 18.f;

            drawText(
                L"贪吃蛇大冒险",
                px, 14.f, 22, white
            );

            drawText(
                L"当前关卡",
                px, 55.f, 15, gray
            );

            drawText(
                to_wstring(level) + L" / 5",
                px, 78.f, 25, white
            );

            drawText(
                L"当前分数",
                px, 118.f, 15, gray
            );

            drawText(
                to_wstring(score),
                px, 141.f, 25, white
            );

            // 通关进度
            drawRect(
                px, 182.f, 218.f, 10.f,
                sf::Color(85, 95, 110)
            );

            drawRect(
                px, 182.f,
                218.f * score / WIN_SCORE,
                10.f,
                sf::Color(75, 220, 135)
            );

            drawText(
                L"怪兽状态",
                px, 211.f, 15, gray
            );

            wstring modeText;

            if (level <= 2)
                modeText = L"巡逻";
            else if (level <= 4)
                modeText = L"巡逻 / 追踪";
            else
                modeText = L"BFS 智能追击";

            drawText(
                modeText,
                px, 235.f, 17, purple
            );

            drawText(
                L"蛇头造型",
                px, 273.f, 15, gray
            );

            wstring headText;

            if (headStyle == HeadStyle::Bunny)
                headText = L"小兔子";
            else if (headStyle == HeadStyle::Bear)
                headText = L"小熊";
            else
                headText = L"小企鹅";

            drawText(
                headText,
                px, 296.f, 17, white
            );

            drawText(
                L"操作说明",
                px, 334.f, 15, gray
            );

            drawText(
                L"WASD / 方向键：移动",
                px, 357.f, 13, white
            );

            drawText(
                L"空格：暂停 / 继续",
                px, 380.f, 13, white
            );

            drawText(
                L"R：重新开始",
                px, 403.f, 13, white
            );

            drawText(
                L"Esc：返回主菜单",
                px, 426.f, 13, white
            );

            drawText(
                L"撞墙、撞自己或蛇头碰怪兽",
                px, 466.f, 12, gray
            );

            // ================= 游戏状态 =================

            if (scene == Scene::Paused ||
                scene == Scene::Dead ||
                scene == Scene::Win)
            {
                drawRect(
                    0.f, 0.f,
                    float(GAME_W),
                    float(GAME_H),
                    sf::Color(0, 0, 0, 175)
                );

                if (scene == Scene::Paused)
                {
                    drawText(
                        L"游戏已暂停",
                        200.f, 175.f, 36,
                        white
                    );

                    drawText(
                        L"按空格继续游戏",
                        220.f, 240.f, 20,
                        white
                    );
                }

                else if (scene == Scene::Dead)
                {
                    drawText(
                        L"游戏结束！",
                        210.f, 175.f, 36,
                        red
                    );

                    drawText(
                        L"按 R 重新开始",
                        225.f, 240.f, 20,
                        white
                    );
                }

                else
                {
                    drawText(
                        L"恭喜通关！",
                        195.f, 175.f, 36,
                        sf::Color(90, 255, 155)
                    );

                    drawText(
                        L"全部五关完成！",
                        215.f, 240.f, 20,
                        white
                    );
                }
            }
        }

        window.display();
    }

    return 0;
}
