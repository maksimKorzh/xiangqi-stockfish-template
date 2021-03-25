# Xiangqi Stockfish Template
Chinese chess xiangqi engine template based on stockfish fork

# What is this?
It's a xiangqi engine template with working array based move generator, perft testing, evaluation and search placeholders.

# What is this created for?
It's created to serve as the bases for the further development of search and evaluation

# Why I no longer maintain it?
I've dropped the development at the stage where the original Stockfishes source code has been modified to generate moves for xiangqi.
The further development assumes either doing things on my own (but it won't be Stockfish any longer after that) or embedding actual
Stockfish's search routines but the way it's done is somewhat completely different compared to the code I used to work with. I just
didn't realize this fact when just started)

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
