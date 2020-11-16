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

(bye)
