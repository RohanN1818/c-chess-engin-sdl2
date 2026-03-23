#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h> 

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 800
#define SQUARE_SIZE (WINDOW_WIDTH / 8)

#define SEARCH_DEPTH 3 
#define INFINITY_SCORE 999999

// --- THE CHESS BOARD STATE ---
char board[8][8] = {
    {'r', 'n', 'b', 'q', 'k', 'b', 'n', 'r'},
    {'p', 'p', 'p', 'p', 'p', 'p', 'p', 'p'},
    {'.', '.', '.', '.', '.', '.', '.', '.'},
    {'.', '.', '.', '.', '.', '.', '.', '.'},
    {'.', '.', '.', '.', '.', '.', '.', '.'},
    {'.', '.', '.', '.', '.', '.', '.', '.'},
    {'P', 'P', 'P', 'P', 'P', 'P', 'P', 'P'},
    {'R', 'N', 'B', 'Q', 'K', 'B', 'N', 'R'}
};

// NEW: Memory to track if pieces have moved (Required for Castling!)
bool hasMoved[8][8] = {false};

bool whiteTurn = true; 

// --- PIXEL ART SPRITES ---
const char* p_art[16] = { "                ", "       ##       ", "      ####      ", "      ####      ", "       ##       ", "      ####      ", "     ######     ", "       ##       ", "       ##       ", "       ##       ", "      ####      ", "     ######     ", "    ########    ", "   ##########   ", "  ############  ", "                " };
const char* r_art[16] = { "                ", "  ##  ####  ##  ", "  ### #### ###  ", "  ############  ", "   ##########   ", "    ########    ", "    ########    ", "    ########    ", "    ########    ", "    ########    ", "   ##########   ", "  ############  ", "  ############  ", " ############## ", " ############## ", "                " };
const char* n_art[16] = { "                ", "       ##       ", "     ######     ", "    ########    ", "   ##########   ", "  ####  ######  ", "  ###    #####  ", "  ###   ######  ", "   ## ########  ", "    ##########  ", "     #########  ", "     ########   ", "    #########   ", "   ##########   ", "  ############  ", "                " };
const char* b_art[16] = { "                ", "       ##       ", "      ####      ", "     ##  ##     ", "    ## ## ##    ", "    ########    ", "     ######     ", "      ####      ", "       ##       ", "      ####      ", "     ######     ", "    ########    ", "   ##########   ", "  ############  ", "  ############  ", "                " };
const char* q_art[16] = { "                ", "  #    ##    #  ", "  ##  ####  ##  ", "  ##  ####  ##  ", "  ### #### ###  ", "   ##########   ", "   ##########   ", "    ########    ", "    ########    ", "     ######     ", "     ######     ", "    ########    ", "   ##########   ", "  ############  ", " ############## ", "                " };
const char* k_art[16] = { "       ##       ", "       ##       ", "     ######     ", "       ##       ", "       ##       ", "   ##########   ", "   ## #### ##   ", "   ##########   ", "    ########    ", "    ########    ", "     ######     ", "    ########    ", "   ##########   ", "  ############  ", " ############## ", "                " };

bool isWhite(char piece) { return piece >= 'A' && piece <= 'Z'; }
bool isBlack(char piece) { return piece >= 'a' && piece <= 'z'; }

// ==========================================
// ====== THE CHESS ENGINE RULES ============
// ==========================================

bool isPathClear(int sr, int sc, int dr, int dc) {
    int rDir = (dr > sr) ? 1 : ((dr < sr) ? -1 : 0);
    int cDir = (dc > sc) ? 1 : ((dc < sc) ? -1 : 0);
    int r = sr + rDir, c = sc + cDir;
    while (r != dr || c != dc) {
        if (board[r][c] != '.') return false; 
        r += rDir; c += cDir;
    }
    return true;
}

bool isPseudoLegalMove(char piece, int sr, int sc, int dr, int dc) {
    char target = board[dr][dc];
    int rDiff = dr - sr;
    int cDiff = dc - sc;
    char type = piece;
    if (type >= 'a' && type <= 'z') type -= 32; 

    if (isWhite(piece) && isWhite(target)) return false;
    if (isBlack(piece) && isBlack(target)) return false;

    if (type == 'P') {
        int dir = (piece == 'P') ? -1 : 1; 
        int startRow = (piece == 'P') ? 6 : 1;
        if (cDiff == 0) {
            if (rDiff == dir && target == '.') return true; 
            if (sr == startRow && rDiff == 2 * dir && target == '.' && board[sr + dir][sc] == '.') return true; 
        }
        if (abs(cDiff) == 1 && rDiff == dir && target != '.') return true; 
        return false;
    }
    if (type == 'N') return ((abs(rDiff) == 2 && abs(cDiff) == 1) || (abs(rDiff) == 1 && abs(cDiff) == 2));
    if (type == 'B') return (abs(rDiff) == abs(cDiff) && isPathClear(sr, sc, dr, dc));
    if (type == 'R') return ((rDiff == 0 || cDiff == 0) && isPathClear(sr, sc, dr, dc));
    if (type == 'Q') return ((abs(rDiff) == abs(cDiff) || rDiff == 0 || cDiff == 0) && isPathClear(sr, sc, dr, dc));
    
    // KING LOGIC (Now includes Castling!)
    if (type == 'K') {
        if (abs(rDiff) <= 1 && abs(cDiff) <= 1) return true;
        
        // Castling
        if (rDiff == 0 && abs(cDiff) == 2) {
            if (hasMoved[sr][sc]) return false; // King has already moved
            
            if (cDiff == 2) { // Kingside (Moving Right)
                if (hasMoved[sr][7] || board[sr][7] != (isWhite(piece) ? 'R' : 'r')) return false;
                if (!isPathClear(sr, sc, sr, 7)) return false; // Path between King and Rook must be empty
            } else { // Queenside (Moving Left)
                if (hasMoved[sr][0] || board[sr][0] != (isWhite(piece) ? 'R' : 'r')) return false;
                if (!isPathClear(sr, sc, sr, 0)) return false;
            }
            return true;
        }
        return false;
    }
    
    return false;
}

bool isKingInCheck(bool forWhite) {
    int kr = -1, kc = -1;
    char kingChar = forWhite ? 'K' : 'k';
    
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (board[r][c] == kingChar) { kr = r; kc = c; break; }
        }
        if (kr != -1) break;
    }
    if (kr == -1) return true; 

    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char enemy = board[r][c];
            if (enemy != '.') {
                if (forWhite && isBlack(enemy) && isPseudoLegalMove(enemy, r, c, kr, kc)) return true;
                if (!forWhite && isWhite(enemy) && isPseudoLegalMove(enemy, r, c, kr, kc)) return true;
            }
        }
    }
    return false;
}

bool isLegalMove(char piece, int sr, int sc, int dr, int dc) {
    if (!isPseudoLegalMove(piece, sr, sc, dr, dc)) return false;

    char captured = board[dr][dc];
    board[dr][dc] = piece;
    board[sr][sc] = '.';
    bool inCheck = isKingInCheck(isWhite(piece));
    board[sr][sc] = piece;
    board[dr][dc] = captured;

    if (inCheck) return false;

    // --- NEW: CASTLING SAFETY CHECKS ---
    char type = piece;
    if (type >= 'a' && type <= 'z') type -= 32;
    
    if (type == 'K' && abs(sc - dc) == 2) {
        if (isKingInCheck(isWhite(piece))) return false; // Can't castle out of check!
        
        int step = (dc > sc) ? 1 : -1;
        board[sr][sc + step] = piece;
        board[sr][sc] = '.';
        bool transitCheck = isKingInCheck(isWhite(piece));
        board[sr][sc] = piece;
        board[sr][sc + step] = '.';
        
        if (transitCheck) return false; // Can't castle through check!
    }

    return true;
}

// ==========================================
// ====== LEVEL 5: MINIMAX AI ENGINE ========
// ==========================================

typedef struct { int sr, sc, dr, dc; } Move;

int evaluateBoard() {
    int score = 0;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            char p = board[r][c];
            if (p == '.') continue;
            
            int val = 0;
            char type = p;
            if (type >= 'a' && type <= 'z') type -= 32;

            if (type == 'P') val = 10;
            else if (type == 'N') val = 30;
            else if (type == 'B') val = 30;
            else if (type == 'R') val = 50;
            else if (type == 'Q') val = 90;
            else if (type == 'K') val = 9000;

            if (r >= 3 && r <= 4 && c >= 3 && c <= 4) val += 2; 

            if (isBlack(p)) score += val;
            else score -= val;
        }
    }
    return score;
}

void generateMoves(bool forBlack, Move moves[], int *count) {
    *count = 0;
    for (int sr = 0; sr < 8; sr++) {
        for (int sc = 0; sc < 8; sc++) {
            char piece = board[sr][sc];
            if ((forBlack && isBlack(piece)) || (!forBlack && isWhite(piece))) {
                for (int dr = 0; dr < 8; dr++) {
                    for (int dc = 0; dc < 8; dc++) {
                        if (isLegalMove(piece, sr, sc, dr, dc)) {
                            moves[*count].sr = sr; moves[*count].sc = sc;
                            moves[*count].dr = dr; moves[*count].dc = dc;
                            (*count)++;
                        }
                    }
                }
            }
        }
    }
}

int minimax(int depth, int alpha, int beta, bool isMaximizingPlayer) {
    if (depth == 0) return evaluateBoard();

    Move moves[1024];
    int moveCount = 0;
    generateMoves(isMaximizingPlayer, moves, &moveCount);

    if (moveCount == 0) {
        if (isKingInCheck(isMaximizingPlayer ? false : true)) return isMaximizingPlayer ? -INFINITY_SCORE : INFINITY_SCORE;
        return 0; 
    }

    if (isMaximizingPlayer) { 
        int maxEval = -INFINITY_SCORE;
        for (int i = 0; i < moveCount; i++) {
            char piece = board[moves[i].sr][moves[i].sc];
            char capturedPiece = board[moves[i].dr][moves[i].dc];
            bool isCastling = (piece == 'K' || piece == 'k') && abs(moves[i].sc - moves[i].dc) == 2;

            // SIMULATE
            board[moves[i].dr][moves[i].dc] = piece;
            board[moves[i].sr][moves[i].sc] = '.';
            if (isCastling) {
                if (moves[i].dc > moves[i].sc) { // Kingside
                    board[moves[i].dr][moves[i].dc - 1] = board[moves[i].dr][7]; board[moves[i].dr][7] = '.';
                } else { // Queenside
                    board[moves[i].dr][moves[i].dc + 1] = board[moves[i].dr][0]; board[moves[i].dr][0] = '.';
                }
            }
            
            bool origMovedSrc = hasMoved[moves[i].sr][moves[i].sc];
            bool origMovedDst = hasMoved[moves[i].dr][moves[i].dc];
            hasMoved[moves[i].sr][moves[i].sc] = true;
            hasMoved[moves[i].dr][moves[i].dc] = true;

            int eval = minimax(depth - 1, alpha, beta, false);

            // UNDO
            board[moves[i].sr][moves[i].sc] = piece;
            board[moves[i].dr][moves[i].dc] = capturedPiece;
            if (isCastling) {
                if (moves[i].dc > moves[i].sc) {
                    board[moves[i].dr][7] = board[moves[i].dr][moves[i].dc - 1]; board[moves[i].dr][moves[i].dc - 1] = '.';
                } else {
                    board[moves[i].dr][0] = board[moves[i].dr][moves[i].dc + 1]; board[moves[i].dr][moves[i].dc + 1] = '.';
                }
            }
            hasMoved[moves[i].sr][moves[i].sc] = origMovedSrc;
            hasMoved[moves[i].dr][moves[i].dc] = origMovedDst;

            if (eval > maxEval) maxEval = eval;
            if (eval > alpha) alpha = eval;
            if (beta <= alpha) break; 
        }
        return maxEval;
    } 
    else { 
        int minEval = INFINITY_SCORE;
        for (int i = 0; i < moveCount; i++) {
            char piece = board[moves[i].sr][moves[i].sc];
            char capturedPiece = board[moves[i].dr][moves[i].dc];
            bool isCastling = (piece == 'K' || piece == 'k') && abs(moves[i].sc - moves[i].dc) == 2;

            board[moves[i].dr][moves[i].dc] = piece;
            board[moves[i].sr][moves[i].sc] = '.';
            if (isCastling) {
                if (moves[i].dc > moves[i].sc) { 
                    board[moves[i].dr][moves[i].dc - 1] = board[moves[i].dr][7]; board[moves[i].dr][7] = '.';
                } else { 
                    board[moves[i].dr][moves[i].dc + 1] = board[moves[i].dr][0]; board[moves[i].dr][0] = '.';
                }
            }

            bool origMovedSrc = hasMoved[moves[i].sr][moves[i].sc];
            bool origMovedDst = hasMoved[moves[i].dr][moves[i].dc];
            hasMoved[moves[i].sr][moves[i].sc] = true;
            hasMoved[moves[i].dr][moves[i].dc] = true;

            int eval = minimax(depth - 1, alpha, beta, true);

            board[moves[i].sr][moves[i].sc] = piece;
            board[moves[i].dr][moves[i].dc] = capturedPiece;
            if (isCastling) {
                if (moves[i].dc > moves[i].sc) {
                    board[moves[i].dr][7] = board[moves[i].dr][moves[i].dc - 1]; board[moves[i].dr][moves[i].dc - 1] = '.';
                } else {
                    board[moves[i].dr][0] = board[moves[i].dr][moves[i].dc + 1]; board[moves[i].dr][moves[i].dc + 1] = '.';
                }
            }
            hasMoved[moves[i].sr][moves[i].sc] = origMovedSrc;
            hasMoved[moves[i].dr][moves[i].dc] = origMovedDst;

            if (eval < minEval) minEval = eval;
            if (eval < beta) beta = eval;
            if (beta <= alpha) break; 
        }
        return minEval;
    }
}

void makeComputerMove() {
    Move moves[1024];
    int moveCount = 0;
    generateMoves(true, moves, &moveCount); 

    int bestScore = -INFINITY_SCORE;
    Move bestMove;
    
    if (moveCount > 0) bestMove = moves[0]; 

    for (int i = 0; i < moveCount; i++) {
        char piece = board[moves[i].sr][moves[i].sc];
        char capturedPiece = board[moves[i].dr][moves[i].dc];
        bool isCastling = (piece == 'K' || piece == 'k') && abs(moves[i].sc - moves[i].dc) == 2;

        board[moves[i].dr][moves[i].dc] = piece;
        board[moves[i].sr][moves[i].sc] = '.';
        if (isCastling) {
            if (moves[i].dc > moves[i].sc) { 
                board[moves[i].dr][moves[i].dc - 1] = board[moves[i].dr][7]; board[moves[i].dr][7] = '.';
            } else { 
                board[moves[i].dr][moves[i].dc + 1] = board[moves[i].dr][0]; board[moves[i].dr][0] = '.';
            }
        }
        
        bool origMovedSrc = hasMoved[moves[i].sr][moves[i].sc];
        bool origMovedDst = hasMoved[moves[i].dr][moves[i].dc];
        hasMoved[moves[i].sr][moves[i].sc] = true;
        hasMoved[moves[i].dr][moves[i].dc] = true;

        int moveScore = minimax(SEARCH_DEPTH - 1, -INFINITY_SCORE, INFINITY_SCORE, false);

        board[moves[i].sr][moves[i].sc] = piece;
        board[moves[i].dr][moves[i].dc] = capturedPiece;
        if (isCastling) {
            if (moves[i].dc > moves[i].sc) {
                board[moves[i].dr][7] = board[moves[i].dr][moves[i].dc - 1]; board[moves[i].dr][moves[i].dc - 1] = '.';
            } else {
                board[moves[i].dr][0] = board[moves[i].dr][moves[i].dc + 1]; board[moves[i].dr][moves[i].dc + 1] = '.';
            }
        }
        hasMoved[moves[i].sr][moves[i].sc] = origMovedSrc;
        hasMoved[moves[i].dr][moves[i].dc] = origMovedDst;

        if (moveScore > bestScore) {
            bestScore = moveScore;
            bestMove = moves[i];
        }
    }

    if (moveCount > 0) {
        char piece = board[bestMove.sr][bestMove.sc];
        bool isCastling = (piece == 'K' || piece == 'k') && abs(bestMove.sc - bestMove.dc) == 2;
        
        board[bestMove.dr][bestMove.dc] = piece;
        board[bestMove.sr][bestMove.sc] = '.';
        
        if (isCastling) {
            if (bestMove.dc > bestMove.sc) { // Kingside
                board[bestMove.dr][bestMove.dc - 1] = board[bestMove.dr][7];
                board[bestMove.dr][7] = '.';
            } else { // Queenside
                board[bestMove.dr][bestMove.dc + 1] = board[bestMove.dr][0];
                board[bestMove.dr][0] = '.';
            }
        }
        
        hasMoved[bestMove.sr][bestMove.sc] = true;
        hasMoved[bestMove.dr][bestMove.dc] = true;
    }
}

// ==========================================

void drawHDpiece(SDL_Renderer* renderer, char piece, int boardCol, int boardRow) {
    if (piece == '.') return;

    char type = piece;
    if (type >= 'a' && type <= 'z') type -= 32;

    const char** bitmap = NULL;
    if (type == 'P') bitmap = p_art; else if (type == 'R') bitmap = r_art;
    else if (type == 'N') bitmap = n_art; else if (type == 'B') bitmap = b_art;
    else if (type == 'Q') bitmap = q_art; else if (type == 'K') bitmap = k_art;
    if (!bitmap) return;

    int pixelSize = SQUARE_SIZE / 20; 
    int offsetX = boardCol * SQUARE_SIZE + (SQUARE_SIZE - (16 * pixelSize)) / 2;
    int offsetY = boardRow * SQUARE_SIZE + (SQUARE_SIZE - (16 * pixelSize)) / 2;

    SDL_SetRenderDrawColor(renderer, 20, 30, 15, 200); 
    for (int r=0; r<16; r++) for (int c=0; c<16; c++) if (bitmap[r][c] == '#') {
        SDL_Rect shadow = { offsetX + c * pixelSize + 4, offsetY + r * pixelSize + 4, pixelSize, pixelSize };
        SDL_RenderFillRect(renderer, &shadow);
    }

    if (piece >= 'A' && piece <= 'Z') SDL_SetRenderDrawColor(renderer, 255, 255, 245, 255);
    else SDL_SetRenderDrawColor(renderer, 35, 35, 35, 255);

    for (int r=0; r<16; r++) for (int c=0; c<16; c++) if (bitmap[r][c] == '#') {
        SDL_Rect pixel = { offsetX + c * pixelSize, offsetY + r * pixelSize, pixelSize, pixelSize };
        SDL_RenderFillRect(renderer, &pixel);
    }
}

int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) return 1;

    SDL_Window* window = SDL_CreateWindow("Level 5 Engine (Castling Enabled!)", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND); 

    bool running = true;
    SDL_Event event;
    int selectedRow = -1;
    int selectedCol = -1;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = false;
            
            else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (whiteTurn) {
                    int clickCol = event.button.x / SQUARE_SIZE;
                    int clickRow = event.button.y / SQUARE_SIZE;
                    char clickedPiece = board[clickRow][clickCol];

                    if (selectedRow == -1) {
                        if (clickedPiece != '.' && isWhite(clickedPiece)) {
                            selectedRow = clickRow; selectedCol = clickCol;
                        }
                    } 
                    else {
                        char movingPiece = board[selectedRow][selectedCol];

                        if (isWhite(movingPiece) && isWhite(clickedPiece)) {
                            selectedRow = clickRow; selectedCol = clickCol;
                        } 
                        else if (isLegalMove(movingPiece, selectedRow, selectedCol, clickRow, clickCol)) {
                            bool isCastling = (toupper(movingPiece) == 'K' && abs(selectedCol - clickCol) == 2);
                            
                            // 1. Move the King
                            board[clickRow][clickCol] = board[selectedRow][selectedCol];
                            board[selectedRow][selectedCol] = '.'; 
                            
                            // 2. Move the Rook automatically!
                            if (isCastling) {
                                if (clickCol > selectedCol) { // Kingside
                                    board[clickRow][clickCol - 1] = board[clickRow][7];
                                    board[clickRow][7] = '.';
                                } else { // Queenside
                                    board[clickRow][clickCol + 1] = board[clickRow][0];
                                    board[clickRow][0] = '.';
                                }
                            }
                            
                            // 3. Mark pieces as moved so they can't castle again
                            hasMoved[selectedRow][selectedCol] = true;
                            hasMoved[clickRow][clickCol] = true;
                            
                            selectedRow = -1; selectedCol = -1;
                            whiteTurn = false; 
                        } 
                        else {
                            selectedRow = -1; selectedCol = -1;
                        }
                    }
                }
            }
        }

        if (!whiteTurn) {
            SDL_RenderPresent(renderer); 
            makeComputerMove();
            whiteTurn = true; 
        }

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);

        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                SDL_Rect square = { col * SQUARE_SIZE, row * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE };
                
                if (row == selectedRow && col == selectedCol) SDL_SetRenderDrawColor(renderer, 246, 246, 105, 255); 
                else if ((row + col) % 2 == 0) SDL_SetRenderDrawColor(renderer, 238, 238, 210, 255); 
                else SDL_SetRenderDrawColor(renderer, 118, 150, 86, 255);  
                
                SDL_RenderFillRect(renderer, &square);
            }
        }

        if (isKingInCheck(true)) {
            for (int r=0; r<8; r++) for (int c=0; c<8; c++) if (board[r][c] == 'K') {
                SDL_Rect kSquare = { c * SQUARE_SIZE, r * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE };
                SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
                SDL_RenderFillRect(renderer, &kSquare);
            }
        }
        if (isKingInCheck(false)) {
            for (int r=0; r<8; r++) for (int c=0; c<8; c++) if (board[r][c] == 'k') {
                SDL_Rect kSquare = { c * SQUARE_SIZE, r * SQUARE_SIZE, SQUARE_SIZE, SQUARE_SIZE };
                SDL_SetRenderDrawColor(renderer, 200, 50, 50, 255);
                SDL_RenderFillRect(renderer, &kSquare);
            }
        }

        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                drawHDpiece(renderer, board[row][col], col, row);
            }
        }

        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}