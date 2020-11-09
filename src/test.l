(de F (X Y) 
    (let A 10 
        (let (AA 20 BB 30)
        (+ A AA BB X Y))))

(prinl (F 1 2))

(do 2
(prinl (F 1 2)))

(bye)
