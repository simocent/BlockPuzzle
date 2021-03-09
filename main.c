#include <raylib.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

#define WIDTH 420
#define HEIGHT 450
#define BLOCKSIZE 20

#define GRID_WIDTH 10
#define GRID_HEIGHT 22

#define X(n) (n%4)
#define Y(n) (n/4)

enum ROTATION{LEFT=-1,RIGHT=1};
enum STATUS{NONE=0,ACTIVE=1,PROJECTION=2};

struct Match{
    uint score;
    uint time;
    uint dropped;
};

struct Block{
    enum STATUS status;
    Color color;
};

struct Tetromino{
    int blocks[4];
    int pivot;
    Color color;
};

static inline void init_stuff();
static void init_game();
static inline void game_loop();
static inline void clear_lines();
static inline bool check_line(int y);
static bool put_tetromino(struct Tetromino tetromino, enum STATUS status, int x, int y);
static void rotate_tetromino(struct Tetromino *tetromino,enum ROTATION rotation);
static void del_blocks(struct Tetromino tetromino, int x, int y, int n);
static inline void del_tetromino(struct Tetromino tetromino, int x, int y);
static struct Tetromino *get_from_queue();
static inline void draw_queue();
static inline void draw_hold(struct Tetromino hold_tetromino);
static int put_projection(struct Tetromino tetromino, bool del_projection, int x, int y);
static inline void check_input(struct Tetromino *tetromino, int *x, int *y, int projection_y, struct Tetromino *hold_tetromino);
static inline void draw_grid();
static inline void draw_text();
static inline void end_game();

static struct Tetromino I_tetromino = {{8,9,10,11},10,SKYBLUE};
static struct Tetromino J_tetromino = {{4,8,9,10},9,BLUE};
static struct Tetromino L_tetromino = {{6,8,9,10},9,ORANGE};
static struct Tetromino O_tetromino = {{5,6,9,10},0,YELLOW};
static struct Tetromino T_tetromino = {{5,8,9,10},9,PURPLE};
static struct Tetromino S_tetromino = {{5,6,8,9},9,GREEN};
static struct Tetromino Z_tetromino = {{4,5,9,10},9,RED};

static struct Tetromino *tetrominoes[] = {&I_tetromino,&J_tetromino,&L_tetromino,&O_tetromino,
    &T_tetromino,&S_tetromino,&Z_tetromino};

static struct Match current_match;
static struct Block grid[GRID_WIDTH][GRID_HEIGHT];
static bool holding_tetromino;
static Texture2D block_texture;
static struct Tetromino *tetromino_queue[6];
static bool next_tetromino;
static bool could_swap;
static int cycle;

static inline void init_stuff(){
    InitWindow(WIDTH, HEIGHT, "Block Puzzle");
    Image image = LoadImage("block.png");
    ImageResize(&image,BLOCKSIZE,BLOCKSIZE);
    block_texture = LoadTextureFromImage(image);
    UnloadImage(image);
    SetTargetFPS(60);
}

static void init_game(){
    cycle=0;
    holding_tetromino=false;
    next_tetromino=true;
    memset(grid,0,sizeof grid);
    memset(tetromino_queue,0,sizeof tetromino_queue);
    current_match.score = current_match.time = current_match.dropped = 0;
}

static inline void game_loop(){
    int x,y;
    int projection_y;
    struct Tetromino hold_tetromino;
    struct Tetromino current_tetromino;
    while (!WindowShouldClose()){
        if(next_tetromino){
            could_swap=true;
            x=(GRID_WIDTH-4)/2;
            y=0;
            current_tetromino=*get_from_queue();
            next_tetromino=false;
        }
        del_tetromino(current_tetromino,x,y);
        put_projection(current_tetromino,true,x,y);
        check_input(&current_tetromino,&x,&y,projection_y,&hold_tetromino);
        projection_y=put_projection(current_tetromino,false,x,y);
        y+=cycle/50;
        cycle%=50;
        if(!put_tetromino(current_tetromino,ACTIVE,x,y)&&cycle==0){
            if(y==1){
                init_game();
                continue;
            }
            ++current_match.dropped;
            put_tetromino(current_tetromino,ACTIVE,x,y-1);
            clear_lines();
            next_tetromino=true;
        }
        
        BeginDrawing();
            ClearBackground(RAYWHITE);
            draw_grid();
            draw_text();
            draw_hold(hold_tetromino);
            draw_queue();
        EndDrawing();
        
        ++cycle;
        ++current_match.time;
        if(IsKeyPressed(KEY_F4))
            init_game();
    }
}

static inline void clear_lines(){
    for(int i=GRID_HEIGHT-1;i>=0;){
        if(check_line(i)){
            ++current_match.score;
            for(int i1=i;i1>0;--i1){
                for(int i2=0;i2<GRID_WIDTH;++i2)
                    grid[i2][i1]=grid[i2][i1-1];
            }
        }
        else
            --i;
    }
}

static inline bool check_line(int y){
    for(int i=0;i<GRID_WIDTH;++i){
        if(grid[i][y].status!=ACTIVE)
            return false;
    }
    return true;
}

static bool put_tetromino(struct Tetromino tetromino, enum STATUS status, int x, int y){
    for(int i=0;i<4;++i){
        int block_x=x+X(tetromino.blocks[i]);
        int block_y=y+Y(tetromino.blocks[i]);
        if(grid[block_x][block_y].status==ACTIVE || block_x<0 || block_x>=GRID_WIDTH || block_y>=GRID_HEIGHT){
            del_blocks(tetromino,x,y,i);
            return false;
        }
        else{
            grid[block_x][block_y].status=status;
            grid[block_x][block_y].color=tetromino.color;
        }
    }
    return true;
}

static void rotate_tetromino(struct Tetromino *tetromino, enum ROTATION rotation){
    if(tetromino->pivot!=0){
        int new_tetromino_blocks[4];
        for(int i=0;i<4;++i){
            int new_x=X(tetromino->pivot)+((Y(tetromino->blocks[i])-Y(tetromino->pivot))*-1*rotation);
            int new_y=Y(tetromino->pivot)+((X(tetromino->blocks[i])-X(tetromino->pivot))*rotation);
            new_tetromino_blocks[i]=new_y*4+new_x;
            if(new_x>=4||new_x<0||new_y>=4||new_y<0){
                rotate_tetromino(tetromino,rotation*-1);
                return;
            }
        }
        memcpy(tetromino->blocks,new_tetromino_blocks,sizeof(tetromino->blocks));
    }
}

static void del_blocks(struct Tetromino tetromino, int x, int y, int n){
    for(int i=0;i<n;++i)
        grid[x+X(tetromino.blocks[i])][y+Y(tetromino.blocks[i])].status = NONE;
}

static inline void del_tetromino(struct Tetromino tetromino, int x, int y){
    del_blocks(tetromino,x,y,4);
}

static struct Tetromino *get_from_queue(){
    for(int i=sizeof(tetromino_queue)/sizeof(tetromino_queue[0])-1;i>=0&&tetromino_queue[i]==NULL;--i)
        tetromino_queue[i]=tetrominoes[GetRandomValue(0,sizeof(tetrominoes)/sizeof(tetrominoes[0])-1)];
    struct Tetromino *next_tetromino=tetromino_queue[0];
    for(int i=0;i<sizeof(tetromino_queue)/sizeof(tetromino_queue[0])-1;++i)
        tetromino_queue[i]=tetromino_queue[i+1];
    tetromino_queue[sizeof(tetromino_queue)/sizeof(tetromino_queue[0])-1]=NULL;
    return next_tetromino;
}

static inline void draw_queue(){
    for(int i1=0;i1<sizeof(tetromino_queue)/sizeof(tetromino_queue[0])-1;++i1){
        for(int i2=0;i2<4;++i2){
            DrawTexture(block_texture,(WIDTH/2+GRID_WIDTH*BLOCKSIZE/2)+10+X(tetromino_queue[i1]->blocks[i2])*BLOCKSIZE,
                (HEIGHT-(GRID_HEIGHT-2)*BLOCKSIZE)/2+i1*4*BLOCKSIZE+Y(tetromino_queue[i1]->blocks[i2])*BLOCKSIZE, tetromino_queue[i1]->color);
        }
    }
}

static inline void draw_hold(struct Tetromino hold_tetromino){
    if(holding_tetromino){
        for(int i=0;i<4;++i){
            DrawTexture(block_texture,(WIDTH/2-(GRID_WIDTH*BLOCKSIZE)/2)-10-4*BLOCKSIZE+X(hold_tetromino.blocks[i])*BLOCKSIZE,
                (HEIGHT-(GRID_HEIGHT-2)*BLOCKSIZE)/2+Y(hold_tetromino.blocks[i])*BLOCKSIZE,
                could_swap?hold_tetromino.color:LIGHTGRAY);
        }
    }
}

static int put_projection(struct Tetromino tetromino, bool del_projection, int x, int y){
    tetromino.color=Fade(tetromino.color,0.5);
    while(put_tetromino(tetromino,PROJECTION,x,++y))
        del_tetromino(tetromino,x,y);
    if(!del_projection)
        put_tetromino(tetromino,PROJECTION,x,--y);
    return y;
}

static inline void check_input(struct Tetromino *tetromino, int *x, int *y,int projection_y, struct Tetromino *hold_tetromino){
    if (IsKeyPressed(KEY_SPACE)){
        *y=projection_y;
        cycle=50;
    }
    else if (IsKeyPressed(KEY_RIGHT)){
        if(put_tetromino(*tetromino,ACTIVE,++(*x),*y))
            del_tetromino(*tetromino,*x,*y);
        else
            --(*x);
    }
    else if (IsKeyPressed(KEY_LEFT)){
        if(put_tetromino(*tetromino,ACTIVE,--(*x),*y))
            del_tetromino(*tetromino,*x,*y);
        else
            ++(*x);
    }
    else if (IsKeyPressed(KEY_UP)){
        rotate_tetromino(tetromino,RIGHT);
        if(!put_tetromino(*tetromino,ACTIVE,*x,*y))
            rotate_tetromino(tetromino,LEFT);
        else
            del_tetromino(*tetromino,*x,*y);
    }
    else if (IsKeyPressed(KEY_RIGHT_SHIFT)){
        rotate_tetromino(tetromino,LEFT);
        if(!put_tetromino(*tetromino,ACTIVE,*x,*y))
            rotate_tetromino(tetromino,RIGHT);
        else
            del_tetromino(*tetromino,*x,*y);
    }
    else if (IsKeyPressed(KEY_C)){
        if(could_swap){
            if(holding_tetromino){
                struct Tetromino temp=*hold_tetromino;
                *hold_tetromino=*tetromino;
                *tetromino=temp;
            }
            else{
                *hold_tetromino=*tetromino;
                *tetromino=*get_from_queue();
                holding_tetromino=true;
            }
            *x=(GRID_WIDTH-4)/2;
            *y=0;
            could_swap=false;
            del_tetromino(*tetromino,*x,*y);
            cycle=0;
        }
    }
    if (IsKeyDown(KEY_DOWN)){
        if(put_tetromino(*tetromino,ACTIVE,*x,(*y)+1)){
            ++(*y);
            cycle=0;
            del_tetromino(*tetromino,*x,*y);
        }
    }
}

static inline void draw_grid(){
    for(int i1=0;i1<GRID_WIDTH;++i1){
        for(int i2=2;i2<GRID_HEIGHT;++i2){
            if(grid[i1][i2].status!=NONE)
                DrawTexture(block_texture,(WIDTH/2-GRID_WIDTH*BLOCKSIZE/2)+i1*BLOCKSIZE,
                            (HEIGHT/2-(GRID_HEIGHT-2)*BLOCKSIZE/2)+(i2-2)*BLOCKSIZE, grid[i1][i2].color);
        }            
    }
    DrawRectangleLines(WIDTH/2-GRID_WIDTH*BLOCKSIZE/2,HEIGHT/2-(GRID_HEIGHT-2)*BLOCKSIZE/2,
                  GRID_WIDTH*BLOCKSIZE,(GRID_HEIGHT-2)*BLOCKSIZE,BLACK); // draw grid outline
}

static inline void draw_text(){
    char score[6];
    sprintf(score,"%u",current_match.score%100000);
    char time[6];
    sprintf(time,"%u",(current_match.time/60)%100000);
    char dropped[6];
    sprintf(dropped,"%u",current_match.dropped%100000);
    DrawText("Score:",10,120,20,RED);
    DrawText(score,10,140,20,BLACK);
    DrawText("Time:",10,165,20,RED);
    DrawText(time,10,185,20,BLACK);
    DrawText("Dropped:",10,210,20,RED);
    DrawText(dropped,10,230,20,BLACK);
}

static inline void end_game(){
    UnloadTexture(block_texture);
    CloseWindow();
}

int main(){
    init_stuff();
    init_game();
    game_loop();
    end_game();
    return 0;
}
