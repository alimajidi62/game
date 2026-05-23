#pragma once
#include <deque>
#include <utility>

// Directions the snake can move
enum class Direction { UP, DOWN, LEFT, RIGHT };

// A single (x, y) cell position on the board
struct Point {
    int x;
    int y;
    bool operator==(const Point& other) const { return x == other.x && y == other.y; }
};

class Snake {
public:
    Snake(int startX, int startY);

    // Move the snake one step in its current direction.
    // Returns the tail segment that was removed (used to erase it from the screen),
    // or {-1,-1} if the snake grew this step (no tail removal needed).
    Point Move(bool grew);

    // Change direction — ignores 180-degree reversals
    void SetDirection(Direction dir);

    Direction      GetDirection()  const { return m_dir; }
    const Point&   GetHead()       const { return m_body.front(); }
    const std::deque<Point>& GetBody() const { return m_body; }

    // Wrap the head position so it stays within [0,boardW) x [0,boardH)
    void WrapHead(int boardW, int boardH);

    // Returns true if the head overlaps any body segment (self-collision)
    bool SelfCollision() const;

private:
    std::deque<Point> m_body; // front = head, back = tail
    Direction         m_dir;
};
