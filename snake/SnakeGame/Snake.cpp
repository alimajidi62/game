#include "Snake.h"
#include <algorithm>

Snake::Snake(int startX, int startY)
    : m_dir(Direction::RIGHT)
{
    // Start with a 3-segment snake pointing right
    m_body.push_back({ startX,     startY });
    m_body.push_back({ startX - 1, startY });
    m_body.push_back({ startX - 2, startY });
}

Point Snake::Move(bool grew)
{
    Point head = m_body.front();

    // Compute the new head position based on current direction
    switch (m_dir) {
    case Direction::UP:    head.y -= 1; break;
    case Direction::DOWN:  head.y += 1; break;
    case Direction::LEFT:  head.x -= 1; break;
    case Direction::RIGHT: head.x += 1; break;
    }

    m_body.push_front(head); // add new head

    if (grew) {
        // Keep the tail — snake grew
        return { -1, -1 };
    }
    else {
        // Remove the old tail and return its position so the renderer can erase it
        Point tail = m_body.back();
        m_body.pop_back();
        return tail;
    }
}

void Snake::SetDirection(Direction dir)
{
    // Prevent 180-degree reversal
    if (m_dir == Direction::UP    && dir == Direction::DOWN)  return;
    if (m_dir == Direction::DOWN  && dir == Direction::UP)    return;
    if (m_dir == Direction::LEFT  && dir == Direction::RIGHT) return;
    if (m_dir == Direction::RIGHT && dir == Direction::LEFT)  return;
    m_dir = dir;
}

void Snake::WrapHead(int boardW, int boardH)
{
    Point& head = m_body.front();
    if (head.x < 0)       head.x = boardW - 1;
    else if (head.x >= boardW) head.x = 0;
    if (head.y < 0)       head.y = boardH - 1;
    else if (head.y >= boardH) head.y = 0;
}

bool Snake::SelfCollision() const
{
    const Point& head = m_body.front();
    // Check every segment except the head itself (index 0)
    for (size_t i = 1; i < m_body.size(); ++i) {
        if (m_body[i] == head) return true;
    }
    return false;
}
