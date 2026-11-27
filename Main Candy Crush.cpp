
#include <SFML/Graphics.hpp>
#include <iostream>
#include <cstdlib>
#include <ctime>

using namespace std;

struct CandyAnimation
{
    // Current visual position
    float x;
    float y;

    // Destination visual position
    float targetX;
    float targetY;
    float startX;
    float startY;

    // Animation progress
    float progress;

    // Whether the candy is animating
    bool isAnimating;
};

// Tracks time for candy animations
sf::Clock animationClock;

// Duration of a candy falling animation
const float ANIMATION_DURATION = 0.3f;

const int ROWS = 9;
const int COLS = 9;

int board[ROWS][COLS];

// Stores the animation info
CandyAnimation candyAnimations[ROWS][COLS];

bool matched[ROWS][COLS];

int selectedRow = -1;
int selectedCol = -1;

int score = 0;

void checkMatches();

void removeMatchedCandies();

void applyGravity();

void refillBoard();

void updateAnimations();

void resolveMatches();

void calculateScore();

bool hasMatches();

void initializeBoard()
{
    
    srand(time(0));

    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            int candy;
            do
            {
                candy = rand() % 5;

            } while (
                (col >= 2 &&
                    board[row][col - 1] == candy &&
                    board[row][col - 2] == candy)
                ||
                (row >= 2 &&
                    board[row - 1][col] == candy &&
                    board[row - 2][col] == candy)
                );
            board[row][col] = candy;
            
            candyAnimations[row][col].x = 62.5f + col * 75.0f + 7.5f;
            candyAnimations[row][col].y = 62.5f + row * 75.0f + 7.5f;
            
            candyAnimations[row][col].startX = candyAnimations[row][col].x;
            candyAnimations[row][col].startY = candyAnimations[row][col].y;

            candyAnimations[row][col].targetX = candyAnimations[row][col].x;
            candyAnimations[row][col].targetY = candyAnimations[row][col].y;
            candyAnimations[row][col].progress = 1.0f;

            candyAnimations[row][col].isAnimating = false;

        }
    }
}

void drawBoard(sf::RenderWindow& window)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");
    sf::Text scoreText(font, "Score: " + to_string(score), 30);
    scoreText.setPosition({ 20.0f, 10.0f });
    scoreText.setFillColor(sf::Color::White);
    window.draw(scoreText);
    float cellSize = 75.0f;
    float startX = 62.5f;
    float startY = 62.5f;

    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            sf::RectangleShape cell({ 73.0f, 73.0f });
            cell.setPosition({
                startX + col * cellSize,
                startY + row * cellSize
                });

            cell.setFillColor(sf::Color(65, 65, 95));
            window.draw(cell);
            if (board[row][col] == -1)
            {
                continue;
            }
            sf::CircleShape candy(30.0f);
            candy.setPosition({
                candyAnimations[row][col].x,
                candyAnimations[row][col].y
                });
            if (board[row][col] == 0)
            {
                candy.setFillColor(sf::Color(252, 108, 133));
            }
            else if (board[row][col] == 1)
            {
                candy.setFillColor(sf::Color(210, 10, 46));
            }
            else if (board[row][col] == 2)
            {
                candy.setFillColor(sf::Color(150, 50, 200));
            }
            else if (board[row][col] == 3)
            {
                candy.setFillColor(sf::Color::Blue);
            }
            else if (board[row][col] == 4)
            {
                candy.setFillColor(sf::Color::Green);
            }

            if (matched[row][col])
            {
                candy.setOutlineThickness(4.0f);
                candy.setOutlineColor(sf::Color::Cyan);
            }
            else if (row == selectedRow && col == selectedCol)
            {
                candy.setOutlineThickness(4.0f);
                candy.setOutlineColor(sf::Color::Yellow);
            }

            window.draw(candy);
        }
    }
}


// Calculate a position between two values, where t is the progress until the end position.
float lerp(float start, float end, float t)
{
    return start + (end - start) * t;
}

// Update all candy animations
void updateAnimations()
{
    // Calculate time elapsed since the previous frame
    float deltaTime = animationClock.restart().asSeconds();

    // Go through every row
    for (int row = 0; row < ROWS; row++)
    {
        // Go through every column
        for (int col = 0; col < COLS; col++)
        {
            // Check whether this candy is animating
            if (candyAnimations[row][col].isAnimating)
            {
                // Increase animation progress
                candyAnimations[row][col].progress += deltaTime / ANIMATION_DURATION;

                // Update the candy's horizontal position
                candyAnimations[row][col].x = lerp(
                    candyAnimations[row][col].startX,
                    candyAnimations[row][col].targetX,
                    candyAnimations[row][col].progress
                );

                // Update the candy's vertical position
                candyAnimations[row][col].y = lerp(
                    candyAnimations[row][col].startY,
                    candyAnimations[row][col].targetY,
                    candyAnimations[row][col].progress
                );

                // Prevent progress from exceeding 1
                if (candyAnimations[row][col].progress >= 1.0f)
                {
                    candyAnimations[row][col].progress = 1.0f;

                    // Animation has finished
                    candyAnimations[row][col].isAnimating = false;
                }
            }
        }
    }
}

void swapCandies(int row1, int col1, int row2, int col2)
{
    int temp = board[row1][col1];
    board[row1][col1] = board[row2][col2];
    board[row2][col2] = temp;
}

void selectCandy(int mouseX, int mouseY)
{
    float cellSize = 75.0f;
    float startX = 62.5f;
    float startY = 62.5f;
    int col = (mouseX - startX) / cellSize;
    int row = (mouseY - startY) / cellSize;
    if (row >= 0 && row < ROWS &&
        col >= 0 && col < COLS)
    {
        if (selectedRow == -1 && selectedCol == -1)
        {
            selectedRow = row;
            selectedCol = col;
        }
        else
        {
            if ((row == selectedRow &&
                (col == selectedCol - 1 || col == selectedCol + 1))
                ||
                (col == selectedCol &&
                    (row == selectedRow - 1 || row == selectedRow + 1)))
            {
                swapCandies(selectedRow, selectedCol, row, col);
                checkMatches();
                if (hasMatches())
                {
                    resolveMatches();
                }
                else
                {
                    swapCandies(selectedRow, selectedCol, row, col);
                }
                selectedRow = -1;
                selectedCol = -1;
            }
            else
            {
                selectedRow = row;
                selectedCol = col;
            }
        }
    }
    else
    {
        selectedRow = -1;
        selectedCol = -1;
    }
}

void checkHorizontalMatches()
{
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS - 2; col++)
        {
            if (board[row][col] == board[row][col + 1] &&
                board[row][col] == board[row][col + 2])
            {
                matched[row][col] = true;
                matched[row][col + 1] = true;
                matched[row][col + 2] = true;
            }
        }
    }
}

void checkVerticalMatches()
{
    for (int col = 0; col < COLS; col++)
    {
        for (int row = 0; row < ROWS - 2; row++)
        {
            if (board[row][col] == board[row + 1][col] &&
                board[row][col] == board[row + 2][col])
            {
                matched[row][col] = true;
                matched[row + 1][col] = true;
                matched[row + 2][col] = true;
            }
        }
    }
}

void checkMatches()
{
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            matched[row][col] = false;
        }
    }

    checkHorizontalMatches();
    checkVerticalMatches();
}

bool hasMatches()
{
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (matched[row][col])
            {
                return true;
            }
        }
    }

    return false;
}

void removeMatchedCandies()
{
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (matched[row][col])
            {
                board[row][col] = -1;
            }
        }
    }
}

void applyGravity()
{
    for (int col = 0; col < COLS; col++)
    {
        // Start at the bottom row
        int writeRow = ROWS - 1;

        // Search upwards through the column
        for (int row = ROWS - 1; row >= 0; row--)
        {
            // Check whether the position contains a candy
            if (board[row][col] != -1)
            {
                // Move the candy down to the available position
                board[writeRow][col] = board[row][col];

                // Move its animation information too
                candyAnimations[writeRow][col] =
                    candyAnimations[row][col];

                // Remember its current visual position
                candyAnimations[writeRow][col].startX =
                    candyAnimations[writeRow][col].x;

                candyAnimations[writeRow][col].startY =
                    candyAnimations[writeRow][col].y;

                // Set its new horizontal destination
                candyAnimations[writeRow][col].targetX =
                    62.5f + col * 75.0f + 7.5f;

                // Set its new vertical destination
                candyAnimations[writeRow][col].targetY =
                    62.5f + writeRow * 75.0f + 7.5f;

                // Reset animation progress
                candyAnimations[writeRow][col].progress = 0.0f;

                // Start animation only if the candy actually moved
                candyAnimations[writeRow][col].isAnimating =
                    (candyAnimations[writeRow][col].startY !=
                        candyAnimations[writeRow][col].targetY);

                // Move the available position upwards
                writeRow--;
            }
        }

        // Fill all remaining positions at the top with -1
        while (writeRow >= 0)
        {
            board[writeRow][col] = -1;
            writeRow--;
        }
    }
}

void refillBoard()
{
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (board[row][col] == -1)
            {
                board[row][col] = rand() % 5;
            }
        }
    }
}

void calculateScore()
{
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (matched[row][col])
            {
                score += 10;
            }
        }
    }
}

void resolveMatches()
{
    while (true)
    {
        checkMatches();
        bool foundMatch = false;
        for (int row = 0; row < ROWS; row++)
        {
            for (int col = 0; col < COLS; col++)
            {
                if (matched[row][col])
                {
                    foundMatch = true;
                }
            }
        }
        if (!foundMatch)
        {
            break;
        }
        calculateScore();
        removeMatchedCandies();
        applyGravity();
        refillBoard();
    }
}

int main()
{
    initializeBoard();
    checkMatches();
    sf::RenderWindow window(
        sf::VideoMode({ 800, 800 }),
        "Candy Crush"
    );

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            if (event->is<sf::Event::MouseButtonPressed>())
            {
                if (event->getIf<sf::Event::MouseButtonPressed>()->button
                    == sf::Mouse::Button::Left)
                {
                    sf::Vector2i mousePosition = sf::Mouse::getPosition(window);

                    selectCandy(mousePosition.x, mousePosition.y);
                }
            }
        }
        updateAnimations();
        window.clear(sf::Color(30, 30, 50));
        drawBoard(window);
        window.display();
    }

    return 0;
}
