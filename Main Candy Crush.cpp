#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <fstream>
#include <string>
#include <optional>
#include <map>
#include <vector>
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

const int ROWS = 9;
const int COLS = 9;
const int STARTING_MOVES = 20;
const int MAX_LEVEL = 5;

// Targets rise while the number of available moves falls.
int getLevelTarget(int level)
{
    const int targets[MAX_LEVEL] = { 300, 400, 500, 600, 700 };
    return targets[level - 1];
}

int getMovesForLevel(int level)
{
    const int moves[MAX_LEVEL] = { 20, 18, 16, 14, 12 };
    return moves[level - 1];
}
const int MAX_LEADERBOARD_ENTRIES = 100;
const int DISPLAYED_LEADERBOARD_ENTRIES = 5;
const float ANIMATION_DURATION = 0.3f;

const string PERSONAL_BEST_FILE = "personal_bests.txt";
const string LEADERBOARD_FILE = "leaderboard.txt";

sf::Clock animationClock;

int board[ROWS][COLS];
CandyAnimation candyAnimations[ROWS][COLS];
bool matched[ROWS][COLS];

// A special candy with a golden "+" badge grants one extra move when matched.
bool moveBonus[ROWS][COLS] = {};
int successfulMoves = 0;
int movesRemaining = STARTING_MOVES;

// Level progress is separate from the total score used by the leaderboard.
// Points beyond a level's target carry over into the next level.
int currentLevel = 1;
int levelScore = 0;       // Score earned in the current level.
int totalScore = 0;       // Cumulative score across every level in this run.
int levelTarget = getLevelTarget(1);
bool gameWon = false;

int selectedRow = -1;
int selectedCol = -1;

string playerName;
int score = 0;
int personalBest = 0;
map<string, int> personalBests;

sf::Texture candyTextures[5];
bool candyTexturesLoaded = false;

LeaderboardEntry leaderboard[MAX_LEADERBOARD_ENTRIES];
int leaderboardCount = 0;

bool nameEntryScreen = true;
bool gameOver = false;
bool scoreSubmitted = false;
bool isResolving = false;
bool isRemovingMatches = false;

bool loadCandyTextures()
{
    const string textureFiles[5] = {
        "assets/watermelon.png",
        "assets/cherry.png",
        "assets/grape.png",
        "assets/blueberry.png",
        "assets/green_apple.png"
    };

    candyTexturesLoaded = true;

    for (int i = 0; i < 5; ++i)
    {
        if (!candyTextures[i].loadFromFile(textureFiles[i]))
        {
            cerr << "Could not load candy texture: " << textureFiles[i]
                << ". Check that the assets folder is beside the program's working directory.\n";
            candyTexturesLoaded = false;
        }
        else
        {
            candyTextures[i].setSmooth(true);
        }
    }
    return candyTexturesLoaded;
}

// SFML 3 audio manager.
struct GameAudio
{
    sf::SoundBuffer selectBuffer, swapBuffer, gameOverBuffer;
    sf::Sound selectSound, swapSound, gameOverSound;
    sf::Music backgroundMusic;

    bool selectLoaded = false;
    bool swapLoaded = false;
    bool gameOverLoaded = false;
    bool musicLoaded = false;

    GameAudio()
        : selectSound(selectBuffer), swapSound(swapBuffer), gameOverSound(gameOverBuffer)
    {
    }

    void loadAudio()
    {
        selectLoaded = selectBuffer.loadFromFile("audio/select.wav");
        swapLoaded = swapBuffer.loadFromFile("audio/swap.wav");
        gameOverLoaded = gameOverBuffer.loadFromFile("audio/gameover.wav");
        musicLoaded = backgroundMusic.openFromFile("audio/background.mp3");

        if (!selectLoaded) cerr << "Could not load audio/select.wav\n";
        if (!swapLoaded) cerr << "Could not load audio/swap.wav\n";
        if (!gameOverLoaded) cerr << "Could not load audio/gameover.wav\n";
        if (!musicLoaded) cerr << "Could not load audio/background.mp3\n";

        selectSound.setVolume(65.0f);
        swapSound.setVolume(75.0f);
        gameOverSound.setVolume(85.0f);

        if (musicLoaded)
        {
            backgroundMusic.setVolume(30.0f);
            backgroundMusic.setLooping(true);
        }
    }

    void playSelect()
    {
        if (selectLoaded) { selectSound.stop(); selectSound.play(); }
    }

    void playSwap()
    {
        if (swapLoaded) { swapSound.stop(); swapSound.play(); }
    }

    void playBackgroundMusic()
    {
        if (musicLoaded &&
            backgroundMusic.getStatus() != sf::SoundSource::Status::Playing)
            backgroundMusic.play();
    }

    void playGameOver()
    {
        if (musicLoaded) backgroundMusic.stop();
        if (gameOverLoaded) { gameOverSound.stop(); gameOverSound.play(); }
    }
};

GameAudio audio;

// Function declarations.
void initializeBoard();
void checkMatches();
void removeMatchedCandies();
void applyGravity();
void refillBoard();
void updateAnimations();
bool allAnimationsFinished();
void resolveMatches();
void calculateScore();
void updateLevelProgress();
bool hasMatches();
void drawGameOver(sf::RenderWindow& window);
void spawnMoveBonus();
void saveLeaderboard();
void finishGame();

// Initialize a board without automatic matches.
void initializeBoard()
{
    for (int row = 0; row < ROWS; ++row)
    {
        for (int col = 0; col < COLS; ++col)
        {
            int candy;
            do
            {
                candy = rand() % 5;
            } while ((col >= 2 && board[row][col - 1] == candy &&
                board[row][col - 2] == candy) ||
                (row >= 2 && board[row - 1][col] == candy &&
                    board[row - 2][col] == candy));

            board[row][col] = candy;
            moveBonus[row][col] = false;

            CandyAnimation& a = candyAnimations[row][col];
            a.x = a.startX = a.targetX = 70.0f + col * 75.0f;
            a.y = a.startY = a.targetY = 70.0f + row * 75.0f;
            a.progress = 1.0f;
            a.scale = a.startScale = a.targetScale = 1.0f;
            a.isAnimating = false;
            matched[row][col] = false;
        }
    }
}

// Place a move-bonus candy on a random occupied cell.
void spawnMoveBonus()
{
    int availableCells = 0;

    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            if (board[row][col] != -1 && !matched[row][col] &&
                !moveBonus[row][col])
                ++availableCells;

    if (availableCells == 0) return;

    int chosenCell = rand() % availableCells;
    for (int row = 0; row < ROWS; ++row)
    {
        for (int col = 0; col < COLS; ++col)
        {
            if (board[row][col] != -1 && !matched[row][col] &&
                !moveBonus[row][col])
            {
                if (chosenCell-- == 0)
                {
                    moveBonus[row][col] = true;
                    return;
                }
            }
        }
    }
}

void drawThemeBackground(sf::RenderWindow& window)
{
    const sf::Color glowColors[5] = {
        sf::Color(255, 75, 115, 18), sf::Color(70, 190, 255, 16),
        sf::Color(170, 90, 255, 17), sf::Color(90, 235, 150, 15),
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

    for (int i = 0; i < 34; ++i)
    {
        float x = static_cast<float>((i * 137 + 41) % 1080);
        float y = static_cast<float>((i * 83 + 29) % 780);
        float radius = 2.0f + static_cast<float>(i % 4);

        sf::CircleShape bubble(radius);
        bubble.setPosition({ x, y });
        bubble.setFillColor(sf::Color(255, 255, 255, 18));
        bubble.setOutlineThickness(1.0f);
        bubble.setOutlineColor(sf::Color(180, 220, 255, 22));
        window.draw(bubble);
    }

}

void drawBoard(sf::RenderWindow& window)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

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

    sf::Text levelText(font,
        "LEVEL " + to_string(currentLevel) + " / " + to_string(MAX_LEVEL) +
        "   TARGET " + to_string(levelTarget),
        19);
    levelText.setFillColor(sf::Color(190, 225, 255));
    sf::FloatRect levelBounds = levelText.getLocalBounds();
    levelText.setPosition({
        405.0f - (levelBounds.position.x + levelBounds.size.x / 2.0f), 16.0f
        });
    window.draw(levelText);

    sf::Text movesText(font, "MOVES  " + to_string(movesRemaining), 21);
    movesText.setFillColor(movesRemaining <= 5
        ? sf::Color(255, 125, 135) : sf::Color(170, 255, 205));
    sf::FloatRect movesBounds = movesText.getLocalBounds();
    movesText.setPosition({
        722.0f - (movesBounds.position.x + movesBounds.size.x), 15.0f
        });
    window.draw(movesText);

    // Progress toward the current level's target.
    sf::RectangleShape progressBackground({ 643.0f, 4.0f });
    progressBackground.setPosition({ 78.0f, 47.0f });
    progressBackground.setFillColor(sf::Color(75, 75, 105));
    window.draw(progressBackground);

    float progress = (levelTarget > 0)
        ? static_cast<float>(levelScore) / static_cast<float>(levelTarget)
        : 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    sf::RectangleShape progressFill({ 643.0f * progress, 4.0f });
    progressFill.setPosition({ 78.0f, 47.0f });
    progressFill.setFillColor(sf::Color(255, 205, 90));
    window.draw(progressFill);

    const float cellSize = 75.0f;
    const float startX = 62.5f;
    const float startY = 62.5f;

    for (int row = 0; row < ROWS; ++row)
    {
        for (int col = 0; col < COLS; ++col)
        {
            sf::RectangleShape cell({ 73.0f, 73.0f });
            cell.setPosition({ startX + col * cellSize, startY + row * cellSize });
            cell.setFillColor(sf::Color(49, 49, 82));
            cell.setOutlineThickness(1.0f);
            cell.setOutlineColor(sf::Color(150, 190, 255, 55));
            window.draw(cell);

            sf::RectangleShape sheen({ 63.0f, 2.0f });
            sheen.setPosition({ startX + col * cellSize + 5.0f,
                                startY + row * cellSize + 6.0f });
            sheen.setFillColor(sf::Color(255, 255, 255, 16));
            window.draw(sheen);

            if (board[row][col] == -1) continue;

            float candyScale = candyAnimations[row][col].scale;
            sf::Vector2f candyCenter(
                candyAnimations[row][col].x + 30.0f,
                candyAnimations[row][col].y + 30.0f
            );

            if (candyTexturesLoaded && board[row][col] >= 0 && board[row][col] < 5)
            {
                sf::Vector2u textureSize = candyTextures[board[row][col]].getSize();
                float widthScale = 64.0f / static_cast<float>(textureSize.x);
                float heightScale = 64.0f / static_cast<float>(textureSize.y);
                float fitScale = (widthScale < heightScale ? widthScale : heightScale);

                sf::Sprite sprite(candyTextures[board[row][col]]);
                sprite.setOrigin({ textureSize.x / 2.0f, textureSize.y / 2.0f });
                sprite.setScale({ fitScale * candyScale, fitScale * candyScale });
                sprite.setPosition(candyCenter);
                window.draw(sprite);
            }
            else
            {
                sf::CircleShape candy(30.0f);
                candy.setOrigin({ 30.0f, 30.0f });
                candy.setScale({ candyScale, candyScale });
                candy.setPosition(candyCenter);

                if (board[row][col] == 0) candy.setFillColor(sf::Color(252, 108, 133));
                else if (board[row][col] == 1) candy.setFillColor(sf::Color(210, 10, 46));
                else if (board[row][col] == 2) candy.setFillColor(sf::Color(150, 50, 200));
                else if (board[row][col] == 3) candy.setFillColor(sf::Color::Blue);
                else candy.setFillColor(sf::Color::Green);
                window.draw(candy);
            }

            sf::CircleShape glint(5.0f);
            glint.setPosition({ candyCenter.x - 16.0f * candyScale,
                                candyCenter.y - 19.0f * candyScale });
            glint.setScale({ 1.0f, 0.62f });
            glint.setFillColor(sf::Color(255, 255, 255, 105));
            window.draw(glint);

            sf::CircleShape tinyGlint(2.2f);
            tinyGlint.setPosition({ candyCenter.x + 9.0f * candyScale,
                                    candyCenter.y - 21.0f * candyScale });
            tinyGlint.setFillColor(sf::Color(255, 255, 255, 85));
            window.draw(tinyGlint);

            if (matched[row][col] || (row == selectedRow && col == selectedCol))
            {
                sf::CircleShape highlight(30.0f);
                highlight.setOrigin({ 30.0f, 30.0f });
                highlight.setPosition(candyCenter);
                highlight.setScale({ candyScale, candyScale });
                highlight.setFillColor(sf::Color::Transparent);
                highlight.setOutlineThickness(3.0f);
                highlight.setOutlineColor(
                    matched[row][col] ? sf::Color::Cyan : sf::Color::Yellow);
                window.draw(highlight);
            }

            // Golden "+" identifies a candy that grants one extra move.
            if (moveBonus[row][col])
            {
                sf::CircleShape badge(11.0f);
                badge.setOrigin({ 11.0f, 11.0f });
                badge.setScale({ candyScale, candyScale });
                badge.setFillColor(sf::Color(255, 215, 0));
                badge.setOutlineThickness(2.0f);
                badge.setOutlineColor(sf::Color::White);
                badge.setPosition({
                    candyAnimations[row][col].x + 52.0f,
                    candyAnimations[row][col].y + 10.0f
                    });
                window.draw(badge);

                sf::Text plusText(font, "+", 17);
                plusText.setFillColor(sf::Color::Black);
                plusText.setScale({ candyScale, candyScale });
                plusText.setPosition({
                    candyAnimations[row][col].x + 46.0f,
                    candyAnimations[row][col].y
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

    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
        {
            CandyAnimation& a = candyAnimations[row][col];
            if (!a.isAnimating) continue;

            a.progress += deltaTime / ANIMATION_DURATION;
            if (a.progress > 1.0f) a.progress = 1.0f;

            a.x = lerp(a.startX, a.targetX, a.progress);
            a.y = lerp(a.startY, a.targetY, a.progress);
            a.scale = lerp(a.startScale, a.targetScale, a.progress);

            if (a.progress >= 1.0f) a.isAnimating = false;
        }
}

bool allAnimationsFinished()
{
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            if (candyAnimations[row][col].isAnimating) return false;
    return true;
}

template <typename T>
void safeSwap(T& first, T& second)
{
    T temporary = first;
    first = second;
    second = temporary;
}

void swapCandies(int row1, int col1, int row2, int col2)
{
    safeSwap(board[row1][col1], board[row2][col2]);
    safeSwap(moveBonus[row1][col1], moveBonus[row2][col2]);
}

void selectCandy(int mouseX, int mouseY)
{
    if (isResolving || movesRemaining <= 0) return;

    const float cellSize = 75.0f;
    const float startX = 62.5f;
    const float startY = 62.5f;

    int col = static_cast<int>((mouseX - startX) / cellSize);
    int row = static_cast<int>((mouseY - startY) / cellSize);

    if (row < 0 || row >= ROWS || col < 0 || col >= COLS)
    {
        selectedRow = selectedCol = -1;
        return;
    }

    if (selectedRow == -1)
    {
        selectedRow = row;
        selectedCol = col;
        audio.playSelect();
        return;
    }

    bool adjacent =
        (row == selectedRow && (col == selectedCol - 1 || col == selectedCol + 1)) ||
        (col == selectedCol && (row == selectedRow - 1 || row == selectedRow + 1));

    if (!adjacent)
    {
        selectedRow = row;
        selectedCol = col;
        audio.playSelect();
        return;
    }

    swapCandies(selectedRow, selectedCol, row, col);
    audio.playSwap();
    checkMatches();

    if (hasMatches())
    {
        // Only a successful player swap consumes a move.
        --movesRemaining;
        ++successfulMoves;

        // A bonus candy appears every six successful swaps.
        if (successfulMoves % 6 == 0)
            spawnMoveBonus();

        resolveMatches();
    }
    else
    {
        // Invalid swaps are reversed and do not cost a move.
        swapCandies(selectedRow, selectedCol, row, col);
    }

    selectedRow = selectedCol = -1;
}

void checkHorizontalMatches()
{
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS - 2; ++col)
            if (board[row][col] != -1 &&
                board[row][col] == board[row][col + 1] &&
                board[row][col] == board[row][col + 2])
            {
                matched[row][col] = true;
                matched[row][col + 1] = true;
                matched[row][col + 2] = true;
            }
}

void checkVerticalMatches()
{
    for (int col = 0; col < COLS; ++col)
        for (int row = 0; row < ROWS - 2; ++row)
            if (board[row][col] != -1 &&
                board[row][col] == board[row + 1][col] &&
                board[row][col] == board[row + 2][col])
            {
                matched[row][col] = true;
                matched[row + 1][col] = true;
                matched[row + 2][col] = true;
            }
}

void checkMatches()
{
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            matched[row][col] = false;

    checkHorizontalMatches();
    checkVerticalMatches();
}

bool hasMatches()
{
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            if (matched[row][col]) return true;
    return false;
}

void removeMatchedCandies()
{
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            if (matched[row][col])
            {
                board[row][col] = -1;
                moveBonus[row][col] = false;
            }
}

void applyGravity()
{
    for (int col = 0; col < COLS; ++col)
    {
        int writeRow = ROWS - 1;

        for (int row = ROWS - 1; row >= 0; --row)
        {
            if (board[row][col] != -1)
            {
                board[writeRow][col] = board[row][col];
                candyAnimations[writeRow][col] = candyAnimations[row][col];
                moveBonus[writeRow][col] = moveBonus[row][col];

                if (writeRow != row) moveBonus[row][col] = false;

                CandyAnimation& a = candyAnimations[writeRow][col];
                a.startX = a.x;
                a.startY = a.y;
                a.targetX = 70.0f + col * 75.0f;
                a.targetY = 70.0f + writeRow * 75.0f;
                a.startScale = a.scale;
                a.targetScale = 1.0f;
                a.progress = 0.0f;
                a.isAnimating = (a.startY != a.targetY);
                --writeRow;
            }
        }

        while (writeRow >= 0)
        {
            board[writeRow][col] = -1;
            moveBonus[writeRow][col] = false;
            --writeRow;
        }
    }
}

void refillBoard()
{
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            if (board[row][col] == -1)
            {
                board[row][col] = rand() % 5;
                moveBonus[row][col] = false;

                CandyAnimation& a = candyAnimations[row][col];
                a.scale = a.startScale = a.targetScale = 1.0f;
                a.targetX = 70.0f + col * 75.0f;
                a.targetY = 70.0f + row * 75.0f;
                a.startX = a.targetX;
                a.startY = a.targetY - (row + 1) * 75.0f - 60.0f;
                a.x = a.startX;
                a.y = a.startY;
                a.progress = 0.0f;
                a.isAnimating = true;
            }
}

// Complete a level, increase the difficulty, and stop after level 5.
// The total run score is preserved for the leaderboard.
void updateLevelProgress()
{
    if (levelScore >= levelTarget)
    {
        if (currentLevel >= MAX_LEVEL)
        {
            // Let the current cascade finish before showing the win screen.
            gameWon = true;
            movesRemaining = 0;
            return;
        }

        ++currentLevel;
        levelScore = 0;
        score = 0;
        movesRemaining = getMovesForLevel(currentLevel);
        successfulMoves = 0;
        levelTarget = getLevelTarget(currentLevel);
    }
}

void calculateScore()
{
    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            if (matched[row][col])
            {
                score += 10;
                levelScore += 10;
                totalScore += 10;

                // Matching a special candy restores one move.
                if (moveBonus[row][col])
                {
                    ++movesRemaining;
                    moveBonus[row][col] = false;
                }
            }

    updateLevelProgress();
}

void resolveMatches()
{
    calculateScore();

    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            if (matched[row][col])
            {
                CandyAnimation& a = candyAnimations[row][col];
                a.startX = a.targetX = a.x;
                a.startY = a.targetY = a.y;
                a.startScale = a.scale;
                a.targetScale = 0.0f;
                a.progress = 0.0f;
                a.isAnimating = true;
            }

    isRemovingMatches = true;
    isResolving = true;
}

string getPerformanceRank()
{
    if (totalScore >= 1000) return "Candy Champion";
    if (totalScore >= 500) return "Sweet Specialist";
    return "Candy Rookie";
}

void sortLeaderboard()
{
    for (int i = 0; i < leaderboardCount - 1; ++i)
        for (int j = 0; j < leaderboardCount - i - 1; ++j)
        {
            bool shouldSwap = leaderboard[j].playerScore <
                leaderboard[j + 1].playerScore;
            if (leaderboard[j].playerScore == leaderboard[j + 1].playerScore &&
                leaderboard[j].playerName > leaderboard[j + 1].playerName)
                shouldSwap = true;

            if (shouldSwap) safeSwap(leaderboard[j], leaderboard[j + 1]);
        }
}

void loadLeaderboard()
{
    leaderboardCount = 0;
    ifstream inputFile(LEADERBOARD_FILE);
    string line;

    if (inputFile.is_open())
    {
        while (getline(inputFile, line) &&
            leaderboardCount < MAX_LEADERBOARD_ENTRIES)
        {
            size_t separator = line.rfind('|');
            if (separator == string::npos) continue;

            string name = line.substr(0, separator);
            string scoreText = line.substr(separator + 1);
            if (name.empty() || name == "Maam Anousha") continue;

            try
            {
                int savedScore = stoi(scoreText);
                bool found = false;

                for (int i = 0; i < leaderboardCount; ++i)
                    if (leaderboard[i].playerName == name)
                    {
                        leaderboard[i].playerScore =
                            (leaderboard[i].playerScore > savedScore ? leaderboard[i].playerScore : savedScore);
                        found = true;
                        break;
                    }

                if (!found)
                {
                    leaderboard[leaderboardCount++] = { name, savedScore };
                }
            }
            catch (...)
            {
                // Ignore malformed leaderboard lines.
            }
        }
    }

    sortLeaderboard();
    saveLeaderboard();
}

void saveLeaderboard()
{
    ofstream outputFile(LEADERBOARD_FILE);
    if (!outputFile.is_open())
    {
        cerr << "Could not save " << LEADERBOARD_FILE << '\n';
        return;
    }

    sortLeaderboard();
    for (int i = 0; i < leaderboardCount; ++i)
        outputFile << leaderboard[i].playerName << "|"
        << leaderboard[i].playerScore << '\n';
}

void submitScoreToLeaderboard()
{
    if (scoreSubmitted || playerName.empty()) return;

    bool found = false;
    for (int i = 0; i < leaderboardCount; ++i)
        if (leaderboard[i].playerName == playerName)
        {
            leaderboard[i].playerScore =
                (leaderboard[i].playerScore > totalScore
                    ? leaderboard[i].playerScore : totalScore);
            found = true;
            break;
        }

    if (!found && leaderboardCount < MAX_LEADERBOARD_ENTRIES)
        leaderboard[leaderboardCount++] = { playerName, totalScore };

    sortLeaderboard();
    saveLeaderboard();
    scoreSubmitted = true;
}

void drawCenteredText(sf::RenderWindow& window, sf::Text& text,
    float centerX, float y)
{
    sf::FloatRect bounds = text.getLocalBounds();
    text.setPosition({
        centerX - (bounds.position.x + bounds.size.x / 2.0f), y
        });
    window.draw(text);
}

void drawLeaderboard(sf::RenderWindow& window, float startY)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

    // Match the deep-indigo, berry-pink and soft-mint title-screen palette.
    sf::RectangleShape panel({ 320.0f, 340.0f });
    panel.setPosition({ 760.0f, startY - 25.0f });
    panel.setFillColor(sf::Color(34, 31, 66, 245));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(160, 180, 220, 190));
    window.draw(panel);

    sf::Text heading(font, "LEADERBOARD", 26);
    heading.setStyle(sf::Text::Bold);
    heading.setLetterSpacing(1.05f);
    heading.setFillColor(sf::Color(255, 145, 180));
    drawCenteredText(window, heading, 920.0f, startY + 5.0f);

    sf::RectangleShape divider({ 270.0f, 2.0f });
    divider.setPosition({ 785.0f, startY + 48.0f });
    divider.setFillColor(sf::Color(145, 165, 205, 170));
    window.draw(divider);

    for (int i = 0; i < DISPLAYED_LEADERBOARD_ENTRIES; ++i)
    {
        string line = (i < leaderboardCount)
            ? to_string(i + 1) + ".  " + leaderboard[i].playerName +
            "  -  " + to_string(leaderboard[i].playerScore)
            : to_string(i + 1) + ".  ---";

        sf::Text entryText(font, line, 18);
        entryText.setFillColor(
            (i < leaderboardCount && leaderboard[i].playerName == playerName)
            ? sf::Color(150, 255, 205) : sf::Color(238, 240, 255));
        entryText.setPosition({ 785.0f, startY + 68.0f + i * 42.0f });
        window.draw(entryText);
    }

    sf::Text credit(font, "PRODUCED BY KODE KRAFTERS", 15);
    credit.setStyle(sf::Text::Bold);
    credit.setFillColor(sf::Color(185, 195, 225));
    drawCenteredText(window, credit, 920.0f, startY + 295.0f);
}

void drawGameOver(sf::RenderWindow& window)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

    sf::RectangleShape overlay({ 1100.0f, 800.0f });
    overlay.setFillColor(sf::Color(10, 9, 27, 225));
    window.draw(overlay);

    // A larger, cleaner results panel with the same palette as the title screen.
    sf::RectangleShape panel({ 540.0f, 610.0f });
    panel.setPosition({ 280.0f, 85.0f });
    panel.setFillColor(sf::Color(34, 31, 66, 250));
    panel.setOutlineThickness(3.0f);
    panel.setOutlineColor(sf::Color(160, 180, 220, 210));
    window.draw(panel);

    sf::RectangleShape topAccent({ 540.0f, 8.0f });
    topAccent.setPosition({ 280.0f, 85.0f });
    topAccent.setFillColor(sf::Color(255, 125, 165));
    window.draw(topAccent);

    sf::Text title(font, gameWon ? "SWEET VICTORY!" : "GAME OVER", 43);
    title.setStyle(sf::Text::Bold);
    title.setLetterSpacing(1.04f);
    title.setFillColor(sf::Color(255, 145, 180));
    drawCenteredText(window, title, 550.0f, 130.0f);

    sf::Text subtitle(font,
        gameWon ? "You completed the Jolly Ranchers challenge!" : "That was a sweet run. Try again!",
        17);
    subtitle.setFillColor(sf::Color(190, 205, 235));
    drawCenteredText(window, subtitle, 550.0f, 190.0f);

    sf::RectangleShape divider({ 390.0f, 2.0f });
    divider.setPosition({ 355.0f, 225.0f });
    divider.setFillColor(sf::Color(145, 165, 205, 170));
    window.draw(divider);

    sf::Text playerText(font,
        playerName + "'s Level Score: " + to_string(score), 23);
    playerText.setFillColor(sf::Color(245, 240, 255));
    drawCenteredText(window, playerText, 550.0f, 250.0f);

    sf::Text totalText(font, "Total Run Score: " + to_string(totalScore), 24);
    totalText.setStyle(sf::Text::Bold);
    totalText.setFillColor(sf::Color(150, 255, 205));
    drawCenteredText(window, totalText, 550.0f, 295.0f);

    string rank = getPerformanceRank();
    sf::Text rankText(font, "RANK  |  " + rank, 24);
    rankText.setStyle(sf::Text::Bold);
    rankText.setFillColor(sf::Color(255, 220, 160));
    drawCenteredText(window, rankText, 550.0f, 345.0f);

    sf::Text levelResultText(font,
        "Level Reached: " + to_string(currentLevel), 22);
    levelResultText.setFillColor(sf::Color(190, 205, 235));
    drawCenteredText(window, levelResultText, 550.0f, 395.0f);

    sf::Text bestText(font, "Personal Best: " + to_string(personalBest), 22);
    bestText.setFillColor(sf::Color(190, 205, 235));
    drawCenteredText(window, bestText, 550.0f, 438.0f);

    sf::Text resultText(font,
        gameWon ? "Congrats! You completed all 5 levels!" : "No moves remaining!",
        20);
    resultText.setStyle(sf::Text::Bold);
    resultText.setFillColor(gameWon
        ? sf::Color(150, 255, 205) : sf::Color(255, 175, 195));
    drawCenteredText(window, resultText, 550.0f, 495.0f);

    sf::RectangleShape restartButton({ 280.0f, 66.0f });
    restartButton.setPosition({ 410.0f, 565.0f });
    restartButton.setFillColor(sf::Color(190, 75, 115));
    restartButton.setOutlineThickness(2.0f);
    restartButton.setOutlineColor(sf::Color(255, 190, 210));
    window.draw(restartButton);

    sf::Text restartText(font, "PLAY AGAIN", 25);
    restartText.setStyle(sf::Text::Bold);
    restartText.setLetterSpacing(1.05f);
    restartText.setFillColor(sf::Color::White);
    drawCenteredText(window, restartText, 550.0f, 582.0f);
}

void drawNameEntry(sf::RenderWindow& window)
{
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

    window.clear(sf::Color(19, 17, 43));
    drawThemeBackground(window);

    sf::RectangleShape panel({ 700.0f, 640.0f });
    panel.setPosition({ 200.0f, 105.0f });
    panel.setFillColor(sf::Color(34, 31, 66, 238));
    panel.setOutlineThickness(2.0f);
    panel.setOutlineColor(sf::Color(160, 205, 255, 105));
    window.draw(panel);

    for (int i = 0; i < 5; ++i)
    {
        if (candyTexturesLoaded)
        {
            sf::Sprite candy(candyTextures[i]);
            sf::Vector2u size = candyTextures[i].getSize();
            float widthScale = 58.0f / static_cast<float>(size.x);
            float heightScale = 58.0f / static_cast<float>(size.y);
            float scale = (widthScale < heightScale ? widthScale : heightScale);
            candy.setOrigin({ size.x / 2.0f, size.y / 2.0f });
            candy.setScale({ scale, scale });
            candy.setPosition({ 302.0f + i * 134.0f, 165.0f });
            window.draw(candy);
        }
    }

    sf::Text titleTop(font, "CANDY CRUSH:", 43);
    titleTop.setLetterSpacing(1.12f);
    titleTop.setFillColor(sf::Color(255, 225, 155));
    drawCenteredText(window, titleTop, 550.0f, 205.0f);

    sf::Text titleBottom(font, "JOLLY RANCHERS", 55);
    titleBottom.setLetterSpacing(1.04f);
    titleBottom.setFillColor(sf::Color(255, 125, 165));
    drawCenteredText(window, titleBottom, 550.0f, 260.0f);

    sf::Text subtitle(font, "A FRUITY, SWEET CANDY CHALLENGE", 20);
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
        ? sf::Color(180, 185, 210) : sf::Color::White);
    drawCenteredText(window, inputText, 550.0f, 477.0f);

    sf::Text instruction(font, "PRESS ENTER TO START", 21);
    instruction.setLetterSpacing(1.08f);
    instruction.setFillColor(sf::Color(150, 255, 205));
    drawCenteredText(window, instruction, 550.0f, 560.0f);

    sf::Text hint(font, "Tip: Match + candies to earn extra moves!", 18);
    hint.setStyle(sf::Text::Bold);
    hint.setFillColor(sf::Color(255, 190, 210));
    drawCenteredText(window, hint, 550.0f, 610.0f);

    // Coordinated, high-contrast credits section.
    sf::RectangleShape creditsPanel({ 610.0f, 96.0f });
    creditsPanel.setPosition({ 245.0f, 645.0f });
    creditsPanel.setFillColor(sf::Color(48, 43, 82, 245));
    creditsPanel.setOutlineThickness(1.5f);
    creditsPanel.setOutlineColor(sf::Color(150, 255, 220, 155));
    window.draw(creditsPanel);

    sf::Text developedBy(font, "DEVELOPED BY", 19);
    developedBy.setStyle(sf::Text::Bold);
    developedBy.setLetterSpacing(1.12f);
    developedBy.setFillColor(sf::Color(150, 255, 220));
    drawCenteredText(window, developedBy, 550.0f, 650.0f);

    sf::Text developerNames(font,
        "Usman Atique  |  Haris Jamshaid  |  Azan Saleem", 21);
    developerNames.setStyle(sf::Text::Bold);
    developerNames.setLetterSpacing(1.02f);
    developerNames.setFillColor(sf::Color(255, 205, 220));
    drawCenteredText(window, developerNames, 550.0f, 675.0f);

    sf::RectangleShape creditDivider({ 470.0f, 1.5f });
    creditDivider.setPosition({ 315.0f, 704.0f });
    creditDivider.setFillColor(sf::Color(155, 165, 215, 150));
    window.draw(creditDivider);

    sf::Text supervisedBy(font, "SUPERVISED BY:  Hussain Malik", 20);
    supervisedBy.setStyle(sf::Text::Bold);
    supervisedBy.setLetterSpacing(1.02f);
    supervisedBy.setFillColor(sf::Color(255, 215, 155));
    drawCenteredText(window, supervisedBy, 550.0f, 710.0f);
}

void loadPersonalBests()
{
    personalBests.clear();
    ifstream inputFile(PERSONAL_BEST_FILE);
    string line;

    if (inputFile.is_open())
        while (getline(inputFile, line))
        {
            size_t separator = line.rfind('|');
            if (separator == string::npos) continue;

            string name = line.substr(0, separator);
            string scoreText = line.substr(separator + 1);
            if (name.empty()) continue;

            try
            {
                int savedScore = stoi(scoreText);
                if (personalBests.find(name) == personalBests.end() ||
                    savedScore > personalBests[name])
                    personalBests[name] = savedScore;
            }
            catch (...)
            {
                // Ignore malformed Personal Best entries.
            }
        }
}

void loadPersonalBest()
{
    personalBest = 0;
    auto record = personalBests.find(playerName);
    if (record != personalBests.end()) personalBest = record->second;
}

void savePersonalBest()
{
    ofstream outputFile(PERSONAL_BEST_FILE);
    if (!outputFile.is_open())
    {
        cerr << "Could not save " << PERSONAL_BEST_FILE << '\n';
        return;
    }

    for (const auto& entry : personalBests)
        outputFile << entry.first << "|" << entry.second << '\n';
}

void finishGame()
{
    if (gameOver) return;

    gameOver = true;
    audio.playGameOver();

    if (totalScore > personalBest)
    {
        personalBest = totalScore;
        personalBests[playerName] = personalBest;
        savePersonalBest();
    }
    submitScoreToLeaderboard();
}

void restartGame()
{
    score = 0;
    totalScore = 0;
    currentLevel = 1;
    levelScore = 0;
    levelTarget = getLevelTarget(currentLevel);
    movesRemaining = getMovesForLevel(currentLevel);
    successfulMoves = 0;
    gameWon = false;

    animationClock.restart();
    selectedRow = selectedCol = -1;

    gameOver = false;
    scoreSubmitted = false;
    isResolving = false;
    isRemovingMatches = false;

    for (int row = 0; row < ROWS; ++row)
        for (int col = 0; col < COLS; ++col)
            moveBonus[row][col] = false;

    initializeBoard();
    checkMatches();
    audio.playBackgroundMusic();
}

int main()
{
    srand(static_cast<unsigned int>(time(nullptr)));

    loadCandyTextures();
    audio.loadAudio();
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
                    if (keyEvent->code == sf::Keyboard::Key::Backspace &&
                        !playerName.empty())
                        playerName.pop_back();
                    else if (keyEvent->code == sf::Keyboard::Key::Enter &&
                        !playerName.empty())
                    {
                        loadPersonalBest();
                        nameEntryScreen = false;
                        restartGame();
                    }
                }
                continue;
            }

            if (gameOver)
            {
                if (const auto* mouseEvent =
                    event->getIf<sf::Event::MouseButtonPressed>())
                {
                    if (mouseEvent->button == sf::Mouse::Button::Left)
                    {
                        sf::Vector2i mousePosition = sf::Mouse::getPosition(window);
                        sf::FloatRect restartBounds(
                            { 420.0f, 535.0f }, { 260.0f, 70.0f });

                        if (restartBounds.contains({
                            static_cast<float>(mousePosition.x),
                            static_cast<float>(mousePosition.y)
                            }))
                            restartGame();
                    }
                }
            }
            else if (const auto* mouseEvent =
                event->getIf<sf::Event::MouseButtonPressed>())
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

        updateAnimations();

        if (!gameOver && isResolving && allAnimationsFinished())
        {
            if (isRemovingMatches)
            {
                removeMatchedCandies();

                for (int row = 0; row < ROWS; ++row)
                    for (int col = 0; col < COLS; ++col)
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
                {
                    isResolving = false;
                    // End only after all cascades finish, so a bonus candy
                    // matched on the final move can still grant an extra move.
                    if (movesRemaining <= 0)
                        finishGame();
                }
            }
        }

        // Also handle a zero-move state when no animation is in progress.
        if (!gameOver && movesRemaining <= 0 && !isResolving)
            finishGame();

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
