#include <SFML/Graphics.hpp>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <cctype>
#include <optional>
#include <map>

using namespace std;

struct CandyAnimation
{
    float x, y;
    float targetX, targetY;
    float startX, startY;
    float scale, startScale, targetScale;
    float progress;
    bool isAnimating;
};

struct LeaderboardEntry
{
    string playerName;
    int playerScore;
};

// Candy animation settings
sf::Clock animationClock;
const float ANIMATION_DURATION = 0.3f;

// Board settings
const int ROWS = 9;
const int COLS = 9;
int board[ROWS][COLS];
CandyAnimation candyAnimations[ROWS][COLS];
bool matched[ROWS][COLS];

// Time Bonus Candy settings
bool timeBonus[ROWS][COLS] = {};
int successfulMatches = 0;

int selectedRow = -1;
int selectedCol = -1;

// Player and score information
string playerName = "";
int score = 0;
int personalBest = 0;

// Individual Personal Best records
map<string, int> personalBests;
const string PERSONAL_BEST_FILE = "personal_bests.txt";

// Jolly Rancher candy artwork: indices match the candy IDs used by board[][].
sf::Texture candyTextures[5];
bool candyTexturesLoaded = false;

bool loadCandyTextures()
{
    const string textureFiles[5] = {
        "assets/watermelon.png", // 0: Watermelon
        "assets/cherry.png",     // 1: Cherry
        "assets/grape.png",      // 2: Grapes
        "assets/blueberry.png",  // 3: Blueberry
        "assets/green_apple.png" // 4: Green Apple
    };

    candyTexturesLoaded = true;

    for (int i = 0; i < 5; i++)
    {
        if (!candyTextures[i].loadFromFile(textureFiles[i]))
        {
            cerr << "Could not load candy texture: " << textureFiles[i]
                << ". Check that the assets folder is beside the program working directory." << endl;
            candyTexturesLoaded = false;
        }
        else
        {
            candyTextures[i].setSmooth(true);
        }
    }

    return candyTexturesLoaded;
}


// Leaderboard settings
const int MAX_LEADERBOARD_ENTRIES = 100;
const int DISPLAYED_LEADERBOARD_ENTRIES = 5;
const string LEADERBOARD_FILE = "leaderboard.txt";
const string SPECIAL_PLAYER_NAME = "Maam Anousha";
const int SPECIAL_PLAYER_SCORE = 1650;
LeaderboardEntry leaderboard[MAX_LEADERBOARD_ENTRIES];
int leaderboardCount = 0;

// Game timer
const float GAME_DURATION_SECONDS = 60.0f;
float timeRemaining = GAME_DURATION_SECONDS;
sf::Clock gameTimerClock;

// Game states
bool nameEntryScreen = true;
bool gameOver = false;
bool scoreSubmitted = false;
bool isResolving = false;
bool isRemovingMatches = false;

// Function declarations
void initializeBoard();
void saveLeaderboard();
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
void spawnTimeBonus();

// Initialize a board without starting with automatic matches.
void initializeBoard()
{
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            int candy;
            do
            {
                candy = rand() % 5;
            } while (
                (col >= 2 && board[row][col - 1] == candy && board[row][col - 2] == candy) ||
                (row >= 2 && board[row - 1][col] == candy && board[row - 2][col] == candy)
                );

            board[row][col] = candy;
            timeBonus[row][col] = false;

            candyAnimations[row][col].x = 70.0f + col * 75.0f;
            candyAnimations[row][col].y = 70.0f + row * 75.0f;
            candyAnimations[row][col].startX = candyAnimations[row][col].x;
            candyAnimations[row][col].startY = candyAnimations[row][col].y;
            candyAnimations[row][col].targetX = candyAnimations[row][col].x;
            candyAnimations[row][col].targetY = candyAnimations[row][col].y;
            candyAnimations[row][col].progress = 1.0f;
            candyAnimations[row][col].scale = 1.0f;
            candyAnimations[row][col].startScale = 1.0f;
            candyAnimations[row][col].targetScale = 1.0f;
            candyAnimations[row][col].isAnimating = false;
            matched[row][col] = false;
        }
    }
}

// Spawn a Time Bonus Candy on a random candy that is not being removed.
void spawnTimeBonus()
{
    int availableCells = 0;

    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (board[row][col] != -1 &&
                !matched[row][col] &&
                !timeBonus[row][col])
            {
                availableCells++;
            }
        }
    }

    if (availableCells == 0)
        return;

    int chosenCell = rand() % availableCells;

    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (board[row][col] != -1 &&
                !matched[row][col] &&
                !timeBonus[row][col])
            {
                if (chosenCell == 0)
                {
                    timeBonus[row][col] = true;
                    return;
                }

                chosenCell--;
            }
        }
    }
}

// Subtle fruit-candy atmosphere: soft color haze, tiny bubbles and sparkles.
// These are lightweight SFML shapes, so no extra image assets are required.
void drawThemeBackground(sf::RenderWindow& window)
{
    // Soft colored glow spots behind the board.
    const sf::Color glowColors[5] = {
        sf::Color(255, 75, 115, 18),
        sf::Color(70, 190, 255, 16),
        sf::Color(170, 90, 255, 17),
        sf::Color(90, 235, 150, 15),
        sf::Color(255, 190, 70, 14)
    };

    for (int i = 0; i < 5; ++i)
    {
        sf::CircleShape glow(145.0f + i * 8.0f);
        glow.setOrigin({ glow.getRadius(), glow.getRadius() });
        glow.setPosition({
            110.0f + static_cast<float>((i * 239) % 950),
            110.0f + static_cast<float>((i * 157) % 610)
            });
        glow.setFillColor(glowColors[i]);
        window.draw(glow);
    }

    // Sparse, low-contrast candy bubbles across the background.
    for (int i = 0; i < 34; ++i)
    {
        const float x = static_cast<float>((i * 137 + 41) % 1080);
        const float y = static_cast<float>((i * 83 + 29) % 780);
        const float radius = 2.0f + static_cast<float>(i % 4);

        sf::CircleShape bubble(radius);
        bubble.setPosition({ x, y });
        bubble.setFillColor(sf::Color(255, 255, 255, 18));
        bubble.setOutlineThickness(1.0f);
        bubble.setOutlineColor(sf::Color(180, 220, 255, 22));
        window.draw(bubble);
    }

    // A faint frame gives the play area a polished candy-wrapper feel.
    sf::RectangleShape frame({ 700.0f, 700.0f });
    frame.setPosition({ 42.0f, 42.0f });
    frame.setFillColor(sf::Color::Transparent);
    frame.setOutlineThickness(2.0f);
    frame.setOutlineColor(sf::Color(150, 205, 255, 45));
    window.draw(frame);
}

void drawBoard(sf::RenderWindow& window)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

    // Unified HUD strip: all three labels sit on one line, aligned to the board.
    sf::RectangleShape hudPanel({ 675.0f, 46.0f });
    hudPanel.setPosition({ 62.5f, 7.0f });
    hudPanel.setFillColor(sf::Color(38, 35, 69, 235));
    hudPanel.setOutlineThickness(1.0f);
    hudPanel.setOutlineColor(sf::Color(150, 205, 255, 75));
    window.draw(hudPanel);

    sf::Text scoreText(font, "SCORE  " + to_string(score), 21);
    scoreText.setFillColor(sf::Color(255, 225, 150));
    scoreText.setPosition({ 78.0f, 15.0f });
    window.draw(scoreText);

    sf::Text nameText(font, playerName, 20);
    nameText.setFillColor(sf::Color(190, 225, 255));
    const sf::FloatRect nameBounds = nameText.getLocalBounds();
    nameText.setPosition({ 400.0f - (nameBounds.position.x + nameBounds.size.x / 2.0f), 16.0f });
    window.draw(nameText);

    sf::Text timerText(font, "TIME  " + to_string(static_cast<int>(ceil(timeRemaining))), 21);
    timerText.setFillColor(timeRemaining <= 10.0f
        ? sf::Color(255, 125, 135)
        : sf::Color(170, 255, 205));
    const sf::FloatRect timerBounds = timerText.getLocalBounds();
    timerText.setPosition({ 722.0f - (timerBounds.position.x + timerBounds.size.x), 15.0f });
    window.draw(timerText);

    const float cellSize = 75.0f;
    const float startX = 62.5f;
    const float startY = 62.5f;

    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            sf::RectangleShape cell({ 73.0f, 73.0f });
            cell.setPosition({ startX + col * cellSize, startY + row * cellSize });
            cell.setFillColor(sf::Color(49, 49, 82));
            cell.setOutlineThickness(1.0f);
            cell.setOutlineColor(sf::Color(150, 190, 255, 55));
            window.draw(cell);

            // Very faint diagonal sheen on each tile.
            sf::RectangleShape tileSheen({ 63.0f, 2.0f });
            tileSheen.setPosition({ startX + col * cellSize + 5.0f,
                                    startY + row * cellSize + 6.0f });
            tileSheen.setFillColor(sf::Color(255, 255, 255, 16));
            window.draw(tileSheen);

            if (board[row][col] == -1)
                continue;

            const float candyScale = candyAnimations[row][col].scale;
            const sf::Vector2f candyCenter(
                candyAnimations[row][col].x + 30.0f,
                candyAnimations[row][col].y + 30.0f
            );

            // Draw the matching Jolly Rancher sprite, preserving its aspect ratio.
            if (candyTexturesLoaded && board[row][col] >= 0 && board[row][col] < 5)
            {
                const sf::Vector2u textureSize = candyTextures[board[row][col]].getSize();
                // Calculate the scale manually (no Algorithm Library needed).
                const float widthScale = 64.0f / static_cast<float>(textureSize.x);
                const float heightScale = 64.0f / static_cast<float>(textureSize.y);
                float fitScale = widthScale;
                if (heightScale < fitScale)
                    fitScale = heightScale;

                sf::Sprite candySprite(candyTextures[board[row][col]]);
                candySprite.setOrigin({
                    textureSize.x / 2.0f,
                    textureSize.y / 2.0f
                    });
                candySprite.setScale({ fitScale * candyScale, fitScale * candyScale });
                candySprite.setPosition(candyCenter);
                window.draw(candySprite);
            }
            else
            {
                // Colored-circle fallback if an image file cannot be loaded.
                sf::CircleShape candy(30.0f);
                candy.setOrigin({ 30.0f, 30.0f });
                candy.setScale({ candyScale, candyScale });
                candy.setPosition(candyCenter);

                if (board[row][col] == 0) candy.setFillColor(sf::Color(252, 108, 133));
                else if (board[row][col] == 1) candy.setFillColor(sf::Color(210, 10, 46));
                else if (board[row][col] == 2) candy.setFillColor(sf::Color(150, 50, 200));
                else if (board[row][col] == 3) candy.setFillColor(sf::Color::Blue);
                else if (board[row][col] == 4) candy.setFillColor(sf::Color::Green);

                window.draw(candy);
            }

            // Small glassy reflection: decorative only, the original candy artwork stays intact.
            sf::CircleShape candyGlint(5.0f);
            candyGlint.setPosition({ candyCenter.x - 16.0f * candyScale,
                                     candyCenter.y - 19.0f * candyScale });
            candyGlint.setScale({ 1.0f, 0.62f });
            candyGlint.setFillColor(sf::Color(255, 255, 255, 105));
            window.draw(candyGlint);

            sf::CircleShape tinyGlint(2.2f);
            tinyGlint.setPosition({ candyCenter.x + 9.0f * candyScale,
                                    candyCenter.y - 21.0f * candyScale });
            tinyGlint.setFillColor(sf::Color(255, 255, 255, 85));
            window.draw(tinyGlint);

            // Keep the existing selection/match highlight visible above the sprite.
            if (matched[row][col] || (row == selectedRow && col == selectedCol))
            {
                sf::CircleShape highlight(30.0f);
                highlight.setOrigin({ 30.0f, 30.0f });
                highlight.setPosition(candyCenter);
                highlight.setScale({ candyScale, candyScale });
                highlight.setFillColor(sf::Color::Transparent);
                highlight.setOutlineThickness(3.0f);
                highlight.setOutlineColor(
                    matched[row][col] ? sf::Color::Cyan : sf::Color::Yellow
                );
                window.draw(highlight);
            }

            // Display a golden + badge on Time Bonus Candies.
            if (timeBonus[row][col])
            {
                float scale = candyAnimations[row][col].scale;

                sf::CircleShape bonusBadge(11.0f);
                bonusBadge.setOrigin({ 11.0f, 11.0f });
                bonusBadge.setScale({ scale, scale });
                bonusBadge.setFillColor(sf::Color(255, 215, 0));
                bonusBadge.setOutlineThickness(2.0f);
                bonusBadge.setOutlineColor(sf::Color::White);
                bonusBadge.setPosition({
                    candyAnimations[row][col].x + 52.0f,
                    candyAnimations[row][col].y + 10.0f
                    });

                window.draw(bonusBadge);

                sf::Text plusText(font, "+", 17);
                plusText.setFillColor(sf::Color::Black);
                plusText.setScale({ scale, scale });
                plusText.setPosition({
                    candyAnimations[row][col].x + 46.0f,
                    candyAnimations[row][col].y + 0.0f
                    });

                window.draw(plusText);
            }
        }
    }
}

float lerp(float start, float end, float t)
{
    return start + (end - start) * t;
}

void updateAnimations()
{
    float deltaTime = animationClock.restart().asSeconds();

    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (!candyAnimations[row][col].isAnimating)
                continue;

            CandyAnimation& animation = candyAnimations[row][col];
            animation.progress += deltaTime / ANIMATION_DURATION;

            if (animation.progress > 1.0f)
                animation.progress = 1.0f;

            animation.x = lerp(animation.startX, animation.targetX, animation.progress);
            animation.y = lerp(animation.startY, animation.targetY, animation.progress);
            animation.scale = lerp(animation.startScale, animation.targetScale, animation.progress);

            if (animation.progress >= 1.0f)
                animation.isAnimating = false;
        }
    }
}

bool allAnimationsFinished()
{
    for (int row = 0; row < ROWS; row++)
        for (int col = 0; col < COLS; col++)
            if (candyAnimations[row][col].isAnimating)
                return false;

    return true;
}

void swapCandies(int row1, int col1, int row2, int col2)
{
    int temp = board[row1][col1];
    board[row1][col1] = board[row2][col2];
    board[row2][col2] = temp;

    // The bonus badge belongs to the candy, so swap it too.
    bool tempBonus = timeBonus[row1][col1];
    timeBonus[row1][col1] = timeBonus[row2][col2];
    timeBonus[row2][col2] = tempBonus;
}

void selectCandy(int mouseX, int mouseY)
{
    if (isResolving || timeRemaining <= 0.0f)
        return;

    const float cellSize = 75.0f;
    const float startX = 62.5f;
    const float startY = 62.5f;

    int col = static_cast<int>((mouseX - startX) / cellSize);
    int row = static_cast<int>((mouseY - startY) / cellSize);

    if (row >= 0 && row < ROWS && col >= 0 && col < COLS)
    {
        if (selectedRow == -1 && selectedCol == -1)
        {
            selectedRow = row;
            selectedCol = col;
        }
        else if ((row == selectedRow && (col == selectedCol - 1 || col == selectedCol + 1)) ||
            (col == selectedCol && (row == selectedRow - 1 || row == selectedRow + 1)))
        {
            swapCandies(selectedRow, selectedCol, row, col);
            checkMatches();

            if (hasMatches())
            {
                // Count successful player moves, not cascade matches.
                successfulMatches++;

                // Spawn a bonus candy after every 5 successful moves.
                if (successfulMatches % 5 == 0)
                    spawnTimeBonus();

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
            if (board[row][col] != -1 &&
                board[row][col] == board[row][col + 1] &&
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
            if (board[row][col] != -1 &&
                board[row][col] == board[row + 1][col] &&
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
        for (int col = 0; col < COLS; col++)
            matched[row][col] = false;

    checkHorizontalMatches();
    checkVerticalMatches();
}

bool hasMatches()
{
    for (int row = 0; row < ROWS; row++)
        for (int col = 0; col < COLS; col++)
            if (matched[row][col])
                return true;

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
                timeBonus[row][col] = false;
            }
        }
    }
}

void applyGravity()
{
    for (int col = 0; col < COLS; col++)
    {
        int writeRow = ROWS - 1;

        for (int row = ROWS - 1; row >= 0; row--)
        {
            if (board[row][col] != -1)
            {
                board[writeRow][col] = board[row][col];
                candyAnimations[writeRow][col] = candyAnimations[row][col];

                // Keep the bonus badge attached to its candy.
                timeBonus[writeRow][col] = timeBonus[row][col];

                if (writeRow != row)
                    timeBonus[row][col] = false;

                candyAnimations[writeRow][col].startX = candyAnimations[writeRow][col].x;
                candyAnimations[writeRow][col].startY = candyAnimations[writeRow][col].y;
                candyAnimations[writeRow][col].targetX = 62.5f + col * 75.0f + 7.5f;
                candyAnimations[writeRow][col].targetY = 62.5f + writeRow * 75.0f + 7.5f;
                candyAnimations[writeRow][col].startScale = candyAnimations[writeRow][col].scale;
                candyAnimations[writeRow][col].targetScale = 1.0f;
                candyAnimations[writeRow][col].progress = 0.0f;

                candyAnimations[writeRow][col].isAnimating =
                    (candyAnimations[writeRow][col].startY != candyAnimations[writeRow][col].targetY);

                writeRow--;
            }
        }

        while (writeRow >= 0)
        {
            board[writeRow][col] = -1;
            timeBonus[writeRow][col] = false;
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
                timeBonus[row][col] = false;

                CandyAnimation& animation = candyAnimations[row][col];
                animation.scale = animation.startScale = animation.targetScale = 1.0f;
                animation.targetX = 62.5f + col * 75.0f + 7.5f;
                animation.targetY = 62.5f + row * 75.0f + 7.5f;
                animation.startX = animation.targetX;
                animation.startY = animation.targetY - (row + 1) * 75.0f - 60.0f;
                animation.x = animation.startX;
                animation.y = animation.startY;
                animation.progress = 0.0f;
                animation.isAnimating = true;
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

                // Matching a Time Bonus Candy awards 5 extra seconds.
                if (timeBonus[row][col])
                {
                    timeRemaining += 5.0f;
                    timeBonus[row][col] = false;
                }
            }
        }
    }
}

void resolveMatches()
{
    calculateScore();

    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            if (matched[row][col])
            {
                CandyAnimation& animation = candyAnimations[row][col];
                animation.startX = animation.targetX = animation.x;
                animation.startY = animation.targetY = animation.y;
                animation.startScale = animation.scale;
                animation.targetScale = 0.0f;
                animation.progress = 0.0f;
                animation.isAnimating = true;
            }
        }
    }

    isRemovingMatches = true;
    isResolving = true;
}

void updateGameTimer()
{
    if (timeRemaining > 0.0f)
    {
        timeRemaining -= gameTimerClock.restart().asSeconds();

        if (timeRemaining < 0.0f)
            timeRemaining = 0.0f;
    }
    else
    {
        gameTimerClock.restart();
    }
}

string getPerformanceRank()
{
    if (score >= 1000) return "Candy Champion";
    if (score >= 500) return "Sweet Specialist";
    return "Candy Rookie";
}

// Keep leaderboard entries ordered from highest to lowest score.
void sortLeaderboard()
{
    for (int i = 0; i < leaderboardCount - 1; i++)
    {
        for (int j = 0; j < leaderboardCount - i - 1; j++)
        {
            bool shouldSwap = false;

            if (leaderboard[j].playerScore < leaderboard[j + 1].playerScore)
                shouldSwap = true;
            else if (leaderboard[j].playerScore == leaderboard[j + 1].playerScore &&
                leaderboard[j].playerName > leaderboard[j + 1].playerName)
                shouldSwap = true;

            if (shouldSwap)
            {
                LeaderboardEntry temp = leaderboard[j];
                leaderboard[j] = leaderboard[j + 1];
                leaderboard[j + 1] = temp;
            }
        }
    }
}

// Add Maam Anousha if she is not already present.
void ensureSpecialPlayerExists()
{
    for (int i = 0; i < leaderboardCount; i++)
    {
        if (leaderboard[i].playerName == SPECIAL_PLAYER_NAME)
            return;
    }

    if (leaderboardCount < MAX_LEADERBOARD_ENTRIES)
    {
        leaderboard[leaderboardCount].playerName = SPECIAL_PLAYER_NAME;
        leaderboard[leaderboardCount].playerScore = SPECIAL_PLAYER_SCORE;
        leaderboardCount++;
    }
}

void loadLeaderboard()
{
    leaderboardCount = 0;
    ifstream inputFile(LEADERBOARD_FILE);
    string line;

    if (inputFile.is_open())
    {
        while (getline(inputFile, line) && leaderboardCount < MAX_LEADERBOARD_ENTRIES)
        {
            size_t separator = line.rfind('|');

            if (separator == string::npos)
                continue;

            string name = line.substr(0, separator);
            string scoreText = line.substr(separator + 1);

            if (name.empty())
                continue;

            try
            {
                int savedScore = stoi(scoreText);
                bool alreadyExists = false;

                for (int i = 0; i < leaderboardCount; i++)
                {
                    if (leaderboard[i].playerName == name)
                    {
                        if (savedScore > leaderboard[i].playerScore)
                            leaderboard[i].playerScore = savedScore;

                        alreadyExists = true;
                        break;
                    }
                }

                if (!alreadyExists)
                {
                    leaderboard[leaderboardCount].playerName = name;
                    leaderboard[leaderboardCount].playerScore = savedScore;
                    leaderboardCount++;
                }
            }
            catch (...)
            {
                // Ignore malformed leaderboard lines.
            }
        }
    }

    ensureSpecialPlayerExists();
    sortLeaderboard();
    saveLeaderboard();
}

void saveLeaderboard()
{
    ofstream outputFile(LEADERBOARD_FILE);

    if (!outputFile.is_open())
    {
        cerr << "Could not save " << LEADERBOARD_FILE << endl;
        return;
    }

    sortLeaderboard();

    for (int i = 0; i < leaderboardCount; i++)
        outputFile << leaderboard[i].playerName << "|" << leaderboard[i].playerScore << '\n';
}

// Update a player's best leaderboard result without creating duplicate names.
void submitScoreToLeaderboard()
{
    if (scoreSubmitted || playerName.empty())
        return;

    bool foundPlayer = false;

    for (int i = 0; i < leaderboardCount; i++)
    {
        if (leaderboard[i].playerName == playerName)
        {
            if (score > leaderboard[i].playerScore)
                leaderboard[i].playerScore = score;

            foundPlayer = true;
            break;
        }
    }

    if (!foundPlayer && leaderboardCount < MAX_LEADERBOARD_ENTRIES)
    {
        leaderboard[leaderboardCount].playerName = playerName;
        leaderboard[leaderboardCount].playerScore = score;
        leaderboardCount++;
    }

    ensureSpecialPlayerExists();
    sortLeaderboard();
    saveLeaderboard();
    scoreSubmitted = true;
}

// Draw text horizontally centered around a given x-coordinate.
void drawCenteredText(sf::RenderWindow& window, sf::Text& text, float centerX, float y)
{
    sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition({
        centerX - (bounds.position.x + bounds.size.x / 2.0f),
        y
        });

    window.draw(text);
}

void drawLeaderboard(sf::RenderWindow& window, float startY)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

    sf::RectangleShape panel({ 320.0f, 340.0f });
    panel.setPosition({ 760.0f, startY - 25.0f });
    panel.setFillColor(sf::Color(45, 45, 70));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(255, 215, 0));
    window.draw(panel);

    sf::Text heading(font, "LEADERBOARD", 26);
    heading.setFillColor(sf::Color(255, 215, 0));
    drawCenteredText(window, heading, 920.0f, startY + 5.0f);

    sf::RectangleShape divider({ 270.0f, 2.0f });
    divider.setPosition({ 785.0f, startY + 48.0f });
    divider.setFillColor(sf::Color(120, 120, 150));
    window.draw(divider);

    for (int i = 0; i < DISPLAYED_LEADERBOARD_ENTRIES; i++)
    {
        string line;
        sf::Color color = sf::Color::White;

        if (i < leaderboardCount)
        {
            line = to_string(i + 1) + ".  " + leaderboard[i].playerName + "  -  " +
                to_string(leaderboard[i].playerScore);

            if (leaderboard[i].playerName == playerName)
                color = sf::Color(120, 255, 150);
            else if (leaderboard[i].playerName == SPECIAL_PLAYER_NAME)
                color = sf::Color(255, 190, 100);
        }
        else
        {
            line = to_string(i + 1) + ".  ---";
        }

        sf::Text entryText(font, line, 18);
        entryText.setFillColor(color);
        entryText.setPosition({ 785.0f, startY + 68.0f + i * 42.0f });
        window.draw(entryText);
    }

    sf::Text challenge(font, "Beat 1650 to take the crown!", 15);
    challenge.setFillColor(sf::Color(255, 190, 100));
    drawCenteredText(window, challenge, 920.0f, startY + 295.0f);
}

void drawGameOver(sf::RenderWindow& window)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

    sf::RectangleShape overlay({ 1100.0f, 800.0f });
    overlay.setFillColor(sf::Color(0, 0, 0, 215));
    window.draw(overlay);

    sf::RectangleShape panel({ 500.0f, 570.0f });
    panel.setPosition({ 300.0f, 100.0f });
    panel.setFillColor(sf::Color(45, 45, 70));
    panel.setOutlineThickness(3.0f);
    panel.setOutlineColor(sf::Color(255, 215, 0));
    window.draw(panel);

    sf::Text title(font, "GAME OVER", 48);
    title.setFillColor(sf::Color::White);
    drawCenteredText(window, title, 550.0f, 145.0f);

    sf::Text playerText(font, playerName + "'s Score: " + to_string(score), 27);
    playerText.setFillColor(sf::Color(255, 220, 100));
    drawCenteredText(window, playerText, 550.0f, 235.0f);

    string rank = getPerformanceRank();

    sf::Text rankText(font, "Rank: " + rank, 25);
    rankText.setFillColor(rank == "Candy Champion" ? sf::Color(255, 215, 0) :
        (rank == "Sweet Specialist" ? sf::Color(100, 220, 255) : sf::Color(150, 255, 150)));
    drawCenteredText(window, rankText, 550.0f, 295.0f);

    sf::Text personalBestText(font, "Personal Best: " + to_string(personalBest), 23);
    personalBestText.setFillColor(sf::Color(255, 180, 100));
    drawCenteredText(window, personalBestText, 550.0f, 350.0f);

    sf::Text timeUpText(font, "Time's up!", 22);
    timeUpText.setFillColor(sf::Color::White);
    drawCenteredText(window, timeUpText, 550.0f, 445.0f);

    sf::RectangleShape restartButton({ 260.0f, 70.0f });
    restartButton.setPosition({ 420.0f, 535.0f });
    restartButton.setFillColor(sf::Color(70, 170, 100));
    restartButton.setOutlineThickness(3.0f);
    restartButton.setOutlineColor(sf::Color::White);
    window.draw(restartButton);

    sf::Text restartText(font, "RESTART GAME", 27);
    restartText.setFillColor(sf::Color::White);
    drawCenteredText(window, restartText, 550.0f, 552.0f);
}

void drawNameEntry(sf::RenderWindow& window)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

    // Reuse the exact soft glow, bubble and frame treatment used behind the game board.
    window.clear(sf::Color(19, 17, 43));
    drawThemeBackground(window);

    // Main translucent-style title panel.
    sf::RectangleShape panel({ 700.0f, 590.0f });
    panel.setPosition({ 200.0f, 105.0f });
    panel.setFillColor(sf::Color(34, 31, 66, 238));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(160, 205, 255, 105));
    window.draw(panel);

    // Colorful candy-like accents around the title.
    const sf::Color candyColors[5] = {
        sf::Color(255, 95, 125),
        sf::Color(255, 185, 75),
        sf::Color(155, 105, 235),
        sf::Color(75, 175, 255),
        sf::Color(100, 220, 145)
    };

    for (int i = 0; i < 5; ++i)
    {
        const float x = 285.0f + i * 132.0f;
        sf::CircleShape candy(17.0f);
        candy.setPosition({ x, 145.0f });
        candy.setFillColor(candyColors[i]);
        candy.setOutlineThickness(2.0f);
        candy.setOutlineColor(sf::Color(255, 255, 255, 150));
        window.draw(candy);

        sf::CircleShape shine(5.0f);
        shine.setPosition({ x + 5.0f, 149.0f });
        shine.setFillColor(sf::Color(255, 255, 255, 165));
        window.draw(shine);
    }

    sf::Text titleTop(font, "CANDY CRUSH:", 43);
    titleTop.setLetterSpacing(1.12f);
    titleTop.setFillColor(sf::Color(255, 225, 155));
    drawCenteredText(window, titleTop, 550.0f, 205.0f);

    sf::Text titleBottom(font, "JOLLY RANCHERS", 55);
    titleBottom.setLetterSpacing(1.04f);
    titleBottom.setFillColor(sf::Color(255, 125, 165));
    drawCenteredText(window, titleBottom, 550.0f, 260.0f);

    // Small cool-toned subtitle and separator.
    sf::Text subtitle(font, "A FRUITY, SWEET MATCH-3 CHALLENGE", 20);
    subtitle.setLetterSpacing(1.08f);
    subtitle.setFillColor(sf::Color(175, 220, 255));
    drawCenteredText(window, subtitle, 550.0f, 340.0f);

    sf::RectangleShape separator({ 390.0f, 2.0f });
    separator.setPosition({ 355.0f, 382.0f });
    separator.setFillColor(sf::Color(170, 205, 255, 100));
    window.draw(separator);

    sf::Text prompt(font, "ENTER YOUR PLAYER NAME", 23);
    prompt.setFillColor(sf::Color(255, 225, 160));
    drawCenteredText(window, prompt, 550.0f, 415.0f);

    sf::RectangleShape nameBox({ 430.0f, 62.0f });
    nameBox.setPosition({ 335.0f, 462.0f });
    nameBox.setFillColor(sf::Color(54, 51, 91));
    nameBox.setOutlineThickness(2.0f);
    nameBox.setOutlineColor(sf::Color(255, 190, 125, 210));
    window.draw(nameBox);

    string shownName = playerName.empty() ? "Type your name here..." : playerName;
    sf::Text inputText(font, shownName, 25);
    inputText.setFillColor(playerName.empty()
        ? sf::Color(180, 185, 210)
        : sf::Color::White);
    drawCenteredText(window, inputText, 550.0f, 477.0f);

    sf::Text instruction(font, "PRESS ENTER TO START", 21);
    instruction.setLetterSpacing(1.08f);
    instruction.setFillColor(sf::Color(150, 255, 205));
    drawCenteredText(window, instruction, 550.0f, 560.0f);

    sf::Text challenge(font, "Can you beat Maam Anousha's 1650 points?", 18);
    challenge.setFillColor(sf::Color(255, 190, 125));
    drawCenteredText(window, challenge, 550.0f, 635.0f);
}

// Load every player's Personal Best from the file.
void loadPersonalBests()
{
    personalBests.clear();

    ifstream inputFile(PERSONAL_BEST_FILE);
    string line;

    if (inputFile.is_open())
    {
        while (getline(inputFile, line))
        {
            size_t separator = line.rfind('|');

            if (separator == string::npos)
                continue;

            string name = line.substr(0, separator);
            string scoreText = line.substr(separator + 1);

            if (name.empty())
                continue;

            try
            {
                int savedScore = stoi(scoreText);

                if (personalBests.find(name) == personalBests.end() ||
                    savedScore > personalBests[name])
                {
                    personalBests[name] = savedScore;
                }
            }
            catch (...)
            {
                // Ignore malformed Personal Best entries.
            }
        }
    }
}

// Retrieve the Personal Best belonging to the current player.
void loadPersonalBest()
{
    personalBest = 0;

    auto playerRecord = personalBests.find(playerName);

    if (playerRecord != personalBests.end())
        personalBest = playerRecord->second;
}

// Save all players' Personal Best records.
void savePersonalBest()
{
    ofstream outputFile(PERSONAL_BEST_FILE);

    if (!outputFile.is_open())
    {
        cerr << "Could not save " << PERSONAL_BEST_FILE << endl;
        return;
    }

    for (const auto& entry : personalBests)
        outputFile << entry.first << "|" << entry.second << '\n';
}

void restartGame()
{
    score = 0;
    timeRemaining = GAME_DURATION_SECONDS;
    successfulMatches = 0;

    gameTimerClock.restart();
    animationClock.restart();

    selectedRow = -1;
    selectedCol = -1;

    gameOver = false;
    scoreSubmitted = false;
    isResolving = false;
    isRemovingMatches = false;

    // Clear all Time Bonus Candies from the previous game.
    for (int row = 0; row < ROWS; row++)
        for (int col = 0; col < COLS; col++)
            timeBonus[row][col] = false;

    initializeBoard();
    checkMatches();
}

int main()
{
    srand(static_cast<unsigned int>(time(nullptr)));

    loadCandyTextures();

    loadPersonalBests();
    loadLeaderboard();

    initializeBoard();
    checkMatches();

    sf::RenderWindow window(sf::VideoMode({ 1100, 800 }), "Candy Crush");
    window.setFramerateLimit(60);

    while (window.isOpen())
    {
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
                window.close();

            if (nameEntryScreen)
            {
                if (const auto* textEvent = event->getIf<sf::Event::TextEntered>())
                {
                    char32_t entered = textEvent->unicode;

                    if (entered >= 32 && entered <= 126 && playerName.size() < 16)
                        playerName += static_cast<char>(entered);
                }

                if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>())
                {
                    if (keyEvent->code == sf::Keyboard::Key::Backspace && !playerName.empty())
                        playerName.pop_back();
                    else if (keyEvent->code == sf::Keyboard::Key::Enter && !playerName.empty())
                    {
                        if (playerName == SPECIAL_PLAYER_NAME)
                            playerName = "Player";

                        loadPersonalBest();

                        nameEntryScreen = false;
                        restartGame();
                    }
                }

                continue;
            }

            if (gameOver)
            {
                if (const auto* mouseEvent = event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouseEvent->button == sf::Mouse::Button::Left)
                    {
                        sf::Vector2i mousePosition = sf::Mouse::getPosition(window);

                        sf::FloatRect restartBounds(
                            { 420.0f, 535.0f },
                            { 260.0f, 70.0f }
                        );

                        if (restartBounds.contains({
                            static_cast<float>(mousePosition.x),
                            static_cast<float>(mousePosition.y)
                            }))
                        {
                            restartGame();
                        }
                    }
                }
            }
            else if (const auto* mouseEvent = event->getIf<sf::Event::MouseButtonPressed>())
            {
                if (mouseEvent->button == sf::Mouse::Button::Left)
                {
                    sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
                    selectCandy(mousePosition.x, mousePosition.y);
                }
            }
        }

        if (nameEntryScreen)
        {
            drawNameEntry(window);
            window.display();
            continue;
        }

        if (!gameOver)
        {
            updateGameTimer();

            if (timeRemaining <= 0.0f)
            {
                timeRemaining = 0.0f;
                gameOver = true;

                if (score > personalBest)
                {
                    personalBest = score;
                    personalBests[playerName] = personalBest;
                    savePersonalBest();
                }

                submitScoreToLeaderboard();
            }
        }

        if (!gameOver)
        {
            updateAnimations();

            if (isResolving && allAnimationsFinished())
            {
                if (isRemovingMatches)
                {
                    removeMatchedCandies();

                    for (int row = 0; row < ROWS; row++)
                        for (int col = 0; col < COLS; col++)
                            matched[row][col] = false;

                    isRemovingMatches = false;

                    applyGravity();
                    refillBoard();
                }
                else
                {
                    checkMatches();

                    if (hasMatches())
                        resolveMatches();
                    else
                        isResolving = false;
                }
            }
        }

        window.clear(sf::Color(19, 17, 43));
        drawThemeBackground(window);

        drawBoard(window);

        if (gameOver)
            drawGameOver(window);
        else
            drawLeaderboard(window, 90.0f);

        window.display();
    }

    return 0;
}
