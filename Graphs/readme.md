`read_connections.cpp` allows to read the info about map from `.csv` file

`draw_graph.cpp` allows to visualise connections.

Make sure to point proper files in `draw_graph.cpp` as pos_file and con_file.

To toggle all connections press T

To compile: 
```
g++ draw_graph.cpp -I "Path to include directory" -L "Path to lib directory" -lSDL2main -lSDL2 -mwindows -o main.exe
```