#include <instant/gdi.hpp>
#include <instant/syscall.hpp>
#include <iostream.hpp>

struct PongGame {
    float ballX, ballY;
    float ballVelX, ballVelY;
    int ballRadius;
    float baseSpeed;
    float speedMultiplier;
    
    float paddle1Y, paddle2Y;
    int paddleWidth, paddleHeight;
    int paddleSpeed;
    
    int score1, score2;
    
    int width, height;
    int framesSinceLastScore;
    
    void init(int w, int h) {
        width = w;
        height = h;
        
        ballRadius = 8;
        ballX = width / 2;
        ballY = height / 2;
        baseSpeed = 4.0f;
        speedMultiplier = 1.0f;
        ballVelX = baseSpeed;
        ballVelY = 3.0f;
        
        paddleWidth = 15;
        paddleHeight = 80;
        paddleSpeed = 5;
        paddle1Y = height / 2 - paddleHeight / 2;
        paddle2Y = height / 2 - paddleHeight / 2;
        
        score1 = 0;
        score2 = 0;
        framesSinceLastScore = 0;
    }
    
    void updatePaddles() {
        float paddle1Center = paddle1Y + paddleHeight / 2;
        if (ballY < paddle1Center - 5) {
            paddle1Y -= paddleSpeed;
        } else if (ballY > paddle1Center + 5) {
            paddle1Y += paddleSpeed;
        }
        
        float paddle2Center = paddle2Y + paddleHeight / 2;
        float predictedY = ballY + ballVelY * 10;
        if (predictedY < paddle2Center - 5) {
            paddle2Y -= paddleSpeed;
        } else if (predictedY > paddle2Center + 5) {
            paddle2Y += paddleSpeed;
        }
        
        if (paddle1Y < 0) paddle1Y = 0;
        if (paddle1Y > height - paddleHeight) paddle1Y = height - paddleHeight;
        if (paddle2Y < 0) paddle2Y = 0;
        if (paddle2Y > height - paddleHeight) paddle2Y = height - paddleHeight;
    }
    
    void updateBall() {
        framesSinceLastScore++;
        if (framesSinceLastScore % 60 == 0) {
            speedMultiplier += 0.05f;
            if (speedMultiplier > 3.0f) speedMultiplier = 3.0f;
        }
        
        ballX += ballVelX * speedMultiplier;
        ballY += ballVelY * speedMultiplier;
        
        if (ballY - ballRadius < 0 || ballY + ballRadius > height) {
            ballVelY = -ballVelY;
        }
        
        if (ballX - ballRadius < paddleWidth + 20 && 
            ballY > paddle1Y && ballY < paddle1Y + paddleHeight) {
            ballVelX = -ballVelX;
            ballX = paddleWidth + 20 + ballRadius;
        }

        if (ballX + ballRadius > width - paddleWidth - 20 && 
            ballY > paddle2Y && ballY < paddle2Y + paddleHeight) {
            ballVelX = -ballVelX;
            ballX = width - paddleWidth - 20 - ballRadius;
        }

        if (ballX < 0) {
            score2++;
            resetBall();
        } else if (ballX > width) {
            score1++;
            resetBall();
        }
    }
    
    void resetBall() {
        ballX = width / 2;
        ballY = height / 2;
        ballVelX = (ballVelX > 0 ? -baseSpeed : baseSpeed);
        ballVelY = 3.0f;
        speedMultiplier = 1.0f;
        framesSinceLastScore = 0;
    }
};

int main() {
    instant::GDI& gdi = instant::GDI::getContext();
    
    if (!gdi.initialize()) {
        printf("GDI initialization failed!\n");
        return 1;
    }
    
    instant::Color white(255, 255, 255);
    instant::Color black(0, 0, 0);
    instant::Color green(0, 255, 0);
    instant::Color cyan(0, 255, 255);
    
    PongGame game;
    game.init(gdi.getWidth(), gdi.getHeight());
    
    while(true){
        gdi.clear(black);
        
        game.updatePaddles();
        game.updateBall();
        
        for (int i = 0; i < game.height; i += 20) {
            gdi.fillRect(game.width / 2 - 2, i, 4, 10, white);
        }

        gdi.fillRect(20, game.paddle1Y, game.paddleWidth, game.paddleHeight, green);
        gdi.fillRect(game.width - 20 - game.paddleWidth, game.paddle2Y, 
                     game.paddleWidth, game.paddleHeight, cyan);
        
        gdi.fillCircle(game.ballX, game.ballY, game.ballRadius, white);
        
        char score1Text[16];
        char score2Text[16];
        
        int s1 = game.score1;
        int s2 = game.score2;
        if (s1 > 999) s1 = 999;
        if (s2 > 999) s2 = 999;
        
        int idx = 0;
        if (s1 >= 100) {
            score1Text[idx++] = '0' + (s1 / 100);
            s1 %= 100;
        }
        if (s1 >= 10 || game.score1 >= 100) {
            score1Text[idx++] = '0' + (s1 / 10);
        }
        score1Text[idx++] = '0' + (s1 % 10);
        score1Text[idx] = '\0';

        idx = 0;
        if (s2 >= 100) {
            score2Text[idx++] = '0' + (s2 / 100);
            s2 %= 100;
        }
        if (s2 >= 10 || game.score2 >= 100) {
            score2Text[idx++] = '0' + (s2 / 10);
        }
        score2Text[idx++] = '0' + (s2 % 10);
        score2Text[idx] = '\0';

        gdi.drawText(game.width / 2 - 60, 30, score1Text, green);
        gdi.drawText(game.width / 2 + 40, 30, score2Text, cyan);

        gdi.swapBuffers();
    }
    
    return 0;
}