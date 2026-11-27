#include <SFML/Graphics.hpp>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cmath>

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
    // Candy size animation
    float scale;
    float startScale;
    float targetScale;
    // Animation progress
    float progress;
    // Whether the candy is animating
    bool isAnimating;
};

// Tracks time for candy animations
sf::Clock animationClock;

// Duration of candy animations
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

// Game timer (Member 2 - Step 11)
const float GAME_DURATION_SECONDS = 60.0f;
float timeRemaining = GAME_DURATION_SECONDS;
sf::Clock gameTimerClock;

// Whether the game has ended.
bool gameOver = false;

// Whether the board is currently resolving matches
bool isResolving = false;

// Whether matched candies are currently shrinking
bool isRemovingMatches = false;

// Function declarations
void checkMatches();
void removeMatchedCandies();
void applyGravity();
void refillBoard();
void updateAnimations();
bool allAnimationsFinished();
void resolveMatches();
void calculateScore();
bool hasMatches();
void updateGameTimer();
void drawGameOver(sf::RenderWindow& window);
string getPerformanceRank();

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
            candyAnimations[row][col].x =
                62.5f + col * 75.0f + 7.5f;
            candyAnimations[row][col].y =
                62.5f + row * 75.0f + 7.5f;
            candyAnimations[row][col].startX =
                candyAnimations[row][col].x;
            candyAnimations[row][col].startY =
                candyAnimations[row][col].y;
            candyAnimations[row][col].targetX =
                candyAnimations[row][col].x;
            candyAnimations[row][col].targetY =
                candyAnimations[row][col].y;
            candyAnimations[row][col].progress = 1.0f;
            // Initialize candy size
            candyAnimations[row][col].scale = 1.0f;
            candyAnimations[row][col].startScale = 1.0f;
            candyAnimations[row][col].targetScale = 1.0f;
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

    // Display the remaining game time.
    sf::Text timerText(font, "Time: " +
        to_string(static_cast<int>(ceil(timeRemaining))), 30);
    timerText.setPosition({ 650.0f, 10.0f });
    timerText.setFillColor(sf::Color::White);
    window.draw(timerText);
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
            // Set the origin to the center of the candy
            candy.setOrigin({ 30.0f, 30.0f });
            // Apply the current animation scale
            candy.setScale({
                candyAnimations[row][col].scale,
                candyAnimations[row][col].scale
                });
            // Position the candy using its center
            candy.setPosition({
                candyAnimations[row][col].x + 30.0f,
                candyAnimations[row][col].y + 30.0f
                });
            // Candy colors
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

// Calculate a position between two values
float lerp(float start, float end, float t)
{
    return start + (end - start) * t;
}
// Update all candy animations
void updateAnimations()
{
    // Calculate time elapsed since the previous frame
    float deltaTime = animationClock.restart().asSeconds();
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (candyAnimations[row][col].isAnimating)
            {
                // Increase animation progress
                candyAnimations[row][col].progress +=
                    deltaTime / ANIMATION_DURATION;
                // Prevent progress from exceeding 1
                if (candyAnimations[row][col].progress > 1.0f)
                {
                    candyAnimations[row][col].progress = 1.0f;
                }
                // Update horizontal position
                candyAnimations[row][col].x = lerp(
                    candyAnimations[row][col].startX,
                    candyAnimations[row][col].targetX,
                    candyAnimations[row][col].progress
                );
                // Update vertical position
                candyAnimations[row][col].y = lerp(
                    candyAnimations[row][col].startY,
                    candyAnimations[row][col].targetY,
                    candyAnimations[row][col].progress
                );
                // Update candy size
                candyAnimations[row][col].scale = lerp(
                    candyAnimations[row][col].startScale,
                    candyAnimations[row][col].targetScale,
                    candyAnimations[row][col].progress
                );
                // Check whether animation has finished
                if (candyAnimations[row][col].progress >= 1.0f)
                {
                    candyAnimations[row][col].isAnimating = false;
                }
            }
        }
    }
}

// Check whether every candy has finished animating
bool allAnimationsFinished()
{
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (candyAnimations[row][col].isAnimating)
            {
                return false;
            }
        }
    }
    return true;
}

void swapCandies(int row1, int col1, int row2, int col2)
{
    int temp = board[row1][col1];
    board[row1][col1] = board[row2][col2];
    board[row2][col2] = temp;
}

void selectCandy(int mouseX, int mouseY)
{
    // Prevent player input during animations
    if (isResolving || timeRemaining <= 0.0f)
    {
        return;
    }
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

                // Prepare size to remain normal
                candyAnimations[writeRow][col].startScale =
                    candyAnimations[writeRow][col].scale;

                candyAnimations[writeRow][col].targetScale = 1.0f;

                // Reset animation progress
                candyAnimations[writeRow][col].progress = 0.0f;

                // Start animation only if the candy actually moved
                candyAnimations[writeRow][col].isAnimating =
                    (candyAnimations[writeRow][col].startY !=
                        candyAnimations[writeRow][col].targetY);

                writeRow--;
            }
        }

        // Fill remaining positions at the top with -1
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
                // Generate a new random candy
                board[row][col] = rand() % 5;
                // Reset new candy to its normal size
                candyAnimations[row][col].scale = 1.0f;
                candyAnimations[row][col].startScale = 1.0f;
                candyAnimations[row][col].targetScale = 1.0f;
                // Set its horizontal destination
                candyAnimations[row][col].targetX =
                    62.5f + col * 75.0f + 7.5f;
                // Set its vertical destination
                candyAnimations[row][col].targetY =
                    62.5f + row * 75.0f + 7.5f;
                // Set its starting horizontal position
                candyAnimations[row][col].startX =
                    candyAnimations[row][col].targetX;
                // Spawn the candy above the game board
                candyAnimations[row][col].startY =
                    candyAnimations[row][col].targetY
                    - (row + 1) * 75.0f - 60.0f;
                // Set the current visual position
                candyAnimations[row][col].x =
                    candyAnimations[row][col].startX;
                candyAnimations[row][col].y =
                    candyAnimations[row][col].startY;
                // Reset animation progress
                candyAnimations[row][col].progress = 0.0f;
                // Start the falling animation
                candyAnimations[row][col].isAnimating = true;
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

// Start one round of match resolution
void resolveMatches()
{
    // Calculate points for the current matches
    calculateScore();
    // Start the shrinking animation for matched candies
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (matched[row][col])
            {
                // Keep the candy stationary while shrinking
                candyAnimations[row][col].startX =
                    candyAnimations[row][col].x;
                candyAnimations[row][col].targetX =
                    candyAnimations[row][col].x;
                candyAnimations[row][col].startY =
                    candyAnimations[row][col].y;
                candyAnimations[row][col].targetY =
                    candyAnimations[row][col].y;
                // Remember the current size
                candyAnimations[row][col].startScale =
                    candyAnimations[row][col].scale;
                // Shrink the candy completely
                candyAnimations[row][col].targetScale = 0.0f;
                // Reset animation progress
                candyAnimations[row][col].progress = 0.0f;
                // Start the disappearance animation
                candyAnimations[row][col].isAnimating = true;
            }
        }
    }

    // Mark the shrinking phase as active
    isRemovingMatches = true;
    // Mark the board as resolving
    isResolving = true;
}

// Update the countdown independently of candy animation timing.
void updateGameTimer()
{
    if (timeRemaining > 0.0f)
    {
        float deltaTime = gameTimerClock.restart().asSeconds();
        timeRemaining -= deltaTime;

        if (timeRemaining < 0.0f)
        {
            timeRemaining = 0.0f;
        }
    }
    else
    {
        // Keep the clock from accumulating time after the countdown ends.
        gameTimerClock.restart();
    }
}

// Display the final screen when the countdown reaches zero.
// Assign a performance title based on the player's final score.
string getPerformanceRank()
{
    // Each matched candy is worth 10 points.
    // These thresholds can be adjusted as gameplay is balanced.
    if (score >= 1000)
    {
        return "Candy Champion";
    }
    else if (score >= 500)
    {
        return "Sweet Specialist";
    }
    else
    {
        return "Candy Rookie";
    }
}

void drawGameOver(sf::RenderWindow& window)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

    // Darken the game board behind the Game Over message.
    sf::RectangleShape overlay({ 800.0f, 800.0f });
    overlay.setPosition({ 0.0f, 0.0f });
    overlay.setFillColor(sf::Color(0, 0, 0, 190));
    window.draw(overlay);

    sf::Text gameOverText(font, "GAME OVER", 64);
    gameOverText.setFillColor(sf::Color::White);
    gameOverText.setPosition({ 225.0f, 285.0f });
    window.draw(gameOverText);

    sf::Text finalScoreText(
        font,
        "Final Score: " + to_string(score),
        36
    );
    finalScoreText.setFillColor(sf::Color(255, 220, 100));
    finalScoreText.setPosition({ 270.0f, 365.0f });
    window.draw(finalScoreText);

    // Display the player's performance title.
    string rank = getPerformanceRank();
    sf::Text rankText(font, "Rank: " + rank, 34);

    if (rank == "Candy Champion")
    {
        rankText.setFillColor(sf::Color(255, 215, 0));
    }
    else if (rank == "Sweet Specialist")
    {
        rankText.setFillColor(sf::Color(100, 220, 255));
    }
    else
    {
        rankText.setFillColor(sf::Color(150, 255, 150));
    }

    rankText.setPosition({ 245.0f, 425.0f });
    window.draw(rankText);

    sf::Text timeUpText(font, "Time's up!", 28);
    timeUpText.setFillColor(sf::Color::White);
    timeUpText.setPosition({ 335.0f, 490.0f });
    window.draw(timeUpText);
}

int main()
{
    initializeBoard();
    checkMatches();
    sf::RenderWindow window(
        sf::VideoMode({ 800, 800 }),
        "Candy Crush"
    );

    // Start the 60-second game countdown.
    gameTimerClock.restart();

    while (window.isOpen())
    {
        // Update the countdown before processing input so moves are blocked
        // as soon as the timer reaches zero.
        if (!gameOver)
        {
            updateGameTimer();

            if (timeRemaining <= 0.0f)
            {
                timeRemaining = 0.0f;
                gameOver = true;
            }
        }

        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }
            if (!gameOver &&
                event->is<sf::Event::MouseButtonPressed>())
            {
                if (event->getIf<sf::Event::MouseButtonPressed>()->button
                    == sf::Mouse::Button::Left)
                {
                    sf::Vector2i mousePosition =
                        sf::Mouse::getPosition(window);

                    selectCandy(mousePosition.x, mousePosition.y);
                }
            }
        }
        // Update all active animations
        updateAnimations();
        // Check whether the board is resolving a cascade
        if (isResolving && allAnimationsFinished())
        {
            // Check whether matched candies have finished shrinking
            if (isRemovingMatches)
            {
                // Remove candies after their disappearance animation
                removeMatchedCandies();
                // Clear previous match information
                for (int row = 0; row < ROWS; row++)
                {
                    for (int col = 0; col < COLS; col++)
                    {
                        matched[row][col] = false;
                    }
                }
                // Finish the shrinking phase
                isRemovingMatches = false;
                // Apply gravity
                applyGravity();
                // Generate new candies
                refillBoard();
            }
            else
            {
                // Check for new matches after falling animations
                checkMatches();
                if (hasMatches())
                {
                    // Start another cascade
                    resolveMatches();
                }
                else
                {
                    // No more matches: finish resolving
                    isResolving = false;
                }
            }
        }

        // Stop animation and cascade updates after the game ends.
        if (!gameOver)
        {
            updateAnimations();

            // Check whether the board is resolving a cascade.
            if (isResolving && allAnimationsFinished())
            {
                // Check whether matched candies have finished shrinking.
                if (isRemovingMatches)
                {
                    // Remove candies after their disappearance animation.
                    removeMatchedCandies();

                    // Clear previous match information.
                    for (int row = 0; row < ROWS; row++)
                    {
                        for (int col = 0; col < COLS; col++)
                        {
                            matched[row][col] = false;
                        }
                    }

                    // Finish the shrinking phase.
                    isRemovingMatches = false;

                    // Apply gravity.
                    applyGravity();

                    // Generate new candies.
                    refillBoard();
                }
                else
                {
                    // Check for new matches after falling animations.
                    checkMatches();

                    if (hasMatches())
                    {
                        // Start another cascade.
                        resolveMatches();
                    }
                    else
                    {
                        // No more matches: finish resolving.
                        isResolving = false;
                    }
                }
            }
        }

        window.clear(sf::Color(30, 30, 50));
        drawBoard(window);

        if (gameOver)
        {
            drawGameOver(window);
        }

        window.display();
    }

    return 0;
}
