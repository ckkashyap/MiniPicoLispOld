
(de diamondShape (X Y S)
    (let (
        W (/ S 2)
        H2 (/ S 2)
        H3 (/ S 3)
        W2 (/ W 2)
        W3 (/ W 3)
        W4 (/ W 4)
        P1X (+ X W)
        P1Y (+ Y (/ S 10))
        P2X (+ X W W)
        P3X (+ X W W W3)
        P3Y (+ Y W3)
        P4X (+ X W W2)
        P4Y (+ Y H3 H2)
        P5X (- (+ X W) W3)

        TopMX1 (+ P1X W3)
        TopMX2 (- P2X W3)
        MidMX1 (+ P5X W3)
        MidMX2 (- P3X W3)

        )

       (sdlLine P1X Y P2X Y)
       (sdlLine P2X Y P3X P3Y)
       (sdlLine P3X P3Y P4X P4Y) 
       (sdlLine P4X P4Y P5X P3Y) 
       (sdlLine P5X P3Y P1X Y) 
       (sdlLine P3X P3Y P5X P3Y) 

       (sdlLine TopMX1 Y MidMX1 P3Y) 
       (sdlLine MidMX1 P3Y P4X P4Y) 

       (sdlLine TopMX2 Y MidMX2 P3Y) 
       (sdlLine MidMX2 P3Y P4X P4Y) 
    )
)

(de diamond (X Y S)
    (diamondShape X Y S)
    #(sdlColor 255 255 255)
    (for F 3 
    (diamondShape (+ X 1 F) (+ Y 1 F) (- S 1 F)))
    )


(de square (X Y S)
    (let ( X1 X X2 (+ X S) Y1 Y Y2 (+ Y S))
        (sdlLine X1 Y1 X1 Y2)
        (sdlLine X1 Y2 X2 Y2)
        (sdlLine X2 Y2 X2 Y1)
        (sdlLine X2 Y1 X1 Y1)

    )
)


(de render ()
    (sdlColor 0 0 0)
    (sdlClear)

    (for I  5
      (for J 6
        (sdlColor 200 200 255)
        #(sdlColor 150 150 255)
        (diamond (* 100 (- I 1) )  (* 100 (- J 1)) 100)
    ))


    (sdlColor 0 255 0)
    (for I 10
        (square (* I 20) 400 12)
     )

    (sdlColor 0 0 255)
    (square 10 500 20)

    (sdlSwap)
)
(sdl 600 600 render)
(bye)

