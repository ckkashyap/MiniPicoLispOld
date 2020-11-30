(de F (X Y) 
    (let A 10 
         (let (AA 20 BB 30)
        (+ A AA BB X Y))))

(prinl (F 1 2))

(prinl (= 1 1))

(if (= 1 1) (if (= 1 2) (prinl 11) (prinl 12)) (prinl 13))
(if (= 1 2) (if (= 1 2) (prinl 11) (prinl 12)) (prinl 13))
(if (= 1 1) (if (= 1 1) (prinl 11) (prinl 12)) (prinl 13))

(do 2
(prinl (F 1 2)))


(prinl 123)
(setq L1 (cons 1 (cons 2 (cons 3 Nil))))
(setq L2 (cdr L1))
(setq L3 (cdr L2))
(prinl (car L1))
(prinl (car L2))
(prinl (car L3))
(prinl 456)


(let X 0 (do 3 (prinl X) (setq X (+ 1 X))))



(setq L4 (make (link 1) (link 2) (link 3) ) )
(prinl (car L4))
(prinl (car (cdr L4)))
(prinl (car (cdr (cdr L4))))

(bye)
