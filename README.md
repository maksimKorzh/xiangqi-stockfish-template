# Xiangqi Stockfish
Chinese chess xiangqi engine fork of stockfish

# Features
 - array based board representation and move generation (11 X 14)
 - heavily simplified code base (no more bitboards, threads, and redudant methods in position class)
 - Proof of concept evaluation for debugging (material + PST)
 - Fail soft negamax search with alpha beta pruning placeholder
 - UCI interaction

# GUI I've been using for debugging (electron js app)
https://github.com/maksimKorzh/ccbridge-arena

# Development progress (documents how I've been changing Stockfish's source code)
https://www.youtube.com/watch?v=7mA7lked_dY&list=PLmN0neTso3Jy3RlOsYwQJiRr1GDdKTKlC
