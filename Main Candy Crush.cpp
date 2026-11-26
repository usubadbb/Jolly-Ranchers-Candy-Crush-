
#include <SFML/Graphics.hpp>
#include <iostream>
#include <cstdlib>
#include <ctime>

using namespace std;

// Board dimensions and are Global Variables
const int ROWS = 9;
const int COLS = 9;

// Stores the candies
int board[ROWS][COLS];

// Stores whether a candy is part of a match
bool matched[ROWS][COLS];

// Stores the currently selected candy, which is -1 when nothing is selected 
int selectedRow = -1;
int selectedCol = -1;

// Stores the player's score
int score = 0;

// Function declaration (prototype as checkMatch lies below selectCandy)
void checkMatches();

// Function declaration or prototype
void removeMatchedCandies();

// Function declaration or prototype
void applyGravity();

// Function declaration or prototype
void refillBoard();

// Function declaration or prototype
void resolveMatches();

// Function declaration or prototype
void calculateScore();

// Function declaration or prototype
bool hasMatches();

// Initialize the board without automatic matches
void initializeBoard()
{
    // Seed the random number generator
    srand(time(0));

    // Go through every row
    for (int row = 0; row < ROWS; row++)
    {
        // Go through every column
        for (int col = 0; col < COLS; col++)
        {
            // Temporary variable for the new candy
            int candy;

            // Keep generating candies until a valid one is found
            do
            {
                // Generate a random candy
                candy = rand() % 5;

            } while (

                // Check for a horizontal match
                (col >= 2 &&
                    board[row][col - 1] == candy &&
                    board[row][col - 2] == candy)

                ||

                // Check for a vertical match
                (row >= 2 &&
                    board[row - 1][col] == candy &&
                    board[row - 2][col] == candy)

                );

            // Place the valid candy on the board
            board[row][col] = candy;
        }
    }
}

// Draw the candy board
void drawBoard(sf::RenderWindow& window)
{
    // Create a font
    static sf::Font font("C:/Windows/Fonts/arial.ttf");

    // Create score text
    sf::Text scoreText(font, "Score: " + to_string(score), 30);

    // Position the score
    scoreText.setPosition({ 20.0f, 10.0f });

    // Set score colour
    scoreText.setFillColor(sf::Color::White);

    // Draw the score
    window.draw(scoreText);

    // Size of each candy cell, a circle with a diameter of 60 pixels will lie in each cell.
    float cellSize = 75.0f;

    // Starting position of the board, centering the grid of candies within the board, leaving a total of 125 pixels horizontally, and vertically.
    float startX = 62.5f;
    float startY = 62.5f;

    // Go through every row
    for (int row = 0; row < ROWS; row++)
    {
        // Go through every column
        for (int col = 0; col < COLS; col++)
        {
            // Create the background square (Leaves a total of two pixels)
            sf::RectangleShape cell({ 73.0f, 73.0f });

            // Position the square
            cell.setPosition({
                startX + col * cellSize,
                startY + row * cellSize
                });

            // Set the square's colour
            cell.setFillColor(sf::Color(65, 65, 95));

            // Draw the square
            window.draw(cell);

            // Skip drawing the candy if the position is empty
            if (board[row][col] == -1)
            {
                continue;
            }

            // Create a circle for the candy
            sf::CircleShape candy(30.0f);

            // Position the candy inside its cell
            candy.setPosition({
                startX + col * cellSize + 7.5f,
                startY + row * cellSize + 7.5f
                });

            // Choose colour based on candy number
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

            // Highlight matched candies
            if (matched[row][col])
            {
                candy.setOutlineThickness(4.0f);
                candy.setOutlineColor(sf::Color::Cyan);
            }

            // Highlight the selected candy
            else if (row == selectedRow && col == selectedCol)
            {
                candy.setOutlineThickness(4.0f);
                candy.setOutlineColor(sf::Color::Yellow);
            }

            // Draw the candy on the window
            window.draw(candy);
        }
    }
}

// Swap the positions of two candies
void swapCandies(int row1, int col1, int row2, int col2)
{
    // Temporarily store the first candy
    int temp = board[row1][col1];

    // Put the second candy in the first position
    board[row1][col1] = board[row2][col2];

    // Put the first candy in the second position
    board[row2][col2] = temp;
}

// Select a candy using mouse coordinates
void selectCandy(int mouseX, int mouseY)
{
    float cellSize = 75.0f;
    float startX = 62.5f;
    float startY = 62.5f;

    // Find the row and column clicked by the mouse
    int col = (mouseX - startX) / cellSize;
    int row = (mouseY - startY) / cellSize;

    // Check if the click is inside the board
    if (row >= 0 && row < ROWS &&
        col >= 0 && col < COLS)
    {
        // If no candy is currently selected
        if (selectedRow == -1 && selectedCol == -1)
        {
            selectedRow = row;
            selectedCol = col;
        }
        else
        {
            // Check whether the second candy is adjacent or one-plus (either same row, or same column)
            if ((row == selectedRow &&
                (col == selectedCol - 1 || col == selectedCol + 1))
                ||
                (col == selectedCol &&
                    (row == selectedRow - 1 || row == selectedRow + 1)))
            {
                // Swap the two candies
                swapCandies(selectedRow, selectedCol, row, col);

                // Detect matches after the swap
                checkMatches();

                // Check whether the swap created a match
                if (hasMatches())
                {
                    // Resolve matches, gravity and cascading reactions
                    resolveMatches();
                }
                else
                {
                    // Reverse the swap because no match was created
                    swapCandies(selectedRow, selectedCol, row, col);
                }

                // Clear the selection after a swap has been made
                selectedRow = -1;
                selectedCol = -1;
            }
            else
            {
                // Select the newly clicked candy instead (without making changes diagonally), any diagonal clicks will result in a selection (Yellow Outline).
                selectedRow = row;
                selectedCol = col;
            }
        }
    }
    else // If a click has been made outside the board
    {
        selectedRow = -1;
        selectedCol = -1;
    }
}

// Detecting horizontal matches
void checkHorizontalMatches()
{
    // Go through every row
    for (int row = 0; row < ROWS; row++)
    {
        // Go through every column, stopping before the last two columns (at 7) because lets say if we're at column 9, the program would look for column 10 and column 11, which do not exist.
        for (int col = 0; col < COLS - 2; col++)
        {
            // Check whether three consecutive candies are identical
            if (board[row][col] == board[row][col + 1] &&
                board[row][col] == board[row][col + 2])
            {
                // Mark all three candies as matched
                matched[row][col] = true;
                matched[row][col + 1] = true;
                matched[row][col + 2] = true;
            }
        }
    }
}

// Detecting vertical matches by reversing the outerloop with columns instead of using rows
void checkVerticalMatches()
{
    // Go through every column
    for (int col = 0; col < COLS; col++)
    {
        // Go through every row, stopping before the last two rows
        for (int row = 0; row < ROWS - 2; row++)
        {
            // Check whether three consecutive candies are identical
            if (board[row][col] == board[row + 1][col] &&
                board[row][col] == board[row + 2][col])
            {
                // Mark all three candies as matched
                matched[row][col] = true;
                matched[row + 1][col] = true;
                matched[row + 2][col] = true;
            }
        }
    }
}

// The Horizontal and Vertical Matches have been merged and will reset each time.
void checkMatches()
{
    // Reset the matched array
    for (int row = 0; row < ROWS; row++)
    {
        for (int col = 0; col < COLS; col++)
        {
            matched[row][col] = false;
        }
    }

    // Detect horizontal matches
    checkHorizontalMatches();

    // Detect vertical matches
    checkVerticalMatches();
}

// Check whether any matches exist
bool hasMatches()
{
    // Go through every row
    for (int row = 0; row < ROWS; row++)
    {
        // Go through every column
        for (int col = 0; col < COLS; col++)
        {
            // Check whether this position is matched
            if (matched[row][col])
            {
                return true;
            }
        }
    }

    // No matches were found
    return false;
}

// Remove matched candies
void removeMatchedCandies()
{
    // Go through every row
    for (int row = 0; row < ROWS; row++)
    {
        // Go through every column
        for (int col = 0; col < COLS; col++)
        {
            // Check whether this candy is matched
            if (matched[row][col])
            {
                // Remove the candy
                board[row][col] = -1;
            }
        }
    }
}

// Applying gravity to the board after removing matched candies.
void applyGravity()
{
    // Go through every column
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

// Refill empty spaces with new candies
void refillBoard()
{
    // Go through every row
    for (int row = 0; row < ROWS; row++)
    {
        // Go through every column
        for (int col = 0; col < COLS; col++)
        {
            // Check whether the position is empty
            if (board[row][col] == -1)
            {
                // Generate a new random candy
                board[row][col] = rand() % 5;
            }
        }
    }
}

// Calculate points for matched candies
void calculateScore()
{
    // Go through every row
    for (int row = 0; row < ROWS; row++)
    {
        // Go through every column
        for (int col = 0; col < COLS; col++)
        {
            // Check whether this candy is matched
            if (matched[row][col])
            {
                // Award 10 points
                score += 10;
            }
        }
    }
}

// Automatically resolve cascading matches
void resolveMatches()
{
    // Keep repeating until no matches remain
    while (true)
    {
        // Detect all current matches
        checkMatches();

        // Check whether at least one match exists
        bool foundMatch = false;

        // Go through every row
        for (int row = 0; row < ROWS; row++)
        {
            // Go through every column
            for (int col = 0; col < COLS; col++)
            {
                // Check whether this position is matched
                if (matched[row][col])
                {
                    foundMatch = true;
                }
            }
        }

        // Stop if no matches were found
        if (!foundMatch)
        {
            break;
        }

        // Calculate points for matched candies
        calculateScore();

        // Remove matching candies
        removeMatchedCandies();

        // Apply gravity
        applyGravity();

        // Refill empty spaces
        refillBoard();
    }
}

int main()
{
    // Generate random candies
    initializeBoard();

    // Detect matches on the board
    checkMatches();

    // Create the game window
    sf::RenderWindow window(
        sf::VideoMode({ 800, 800 }),
        "Candy Crush"
    );

    // Main game loop
    while (window.isOpen())
    {
        // Check window events
        while (const std::optional event = window.pollEvent())
        {
            if (event->is<sf::Event::Closed>())
            {
                window.close();
            }

            // Detect mouse clicks
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

        // Clear the window
        window.clear(sf::Color(30, 30, 50));

        // Draw the candy board
        drawBoard(window);

        // Display everything
        window.display();
    }

    return 0;
}
