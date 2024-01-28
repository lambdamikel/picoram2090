(defun sin-d (x) (sin (* (/ x 180) pi)))

(defun cos-d (x) (cos (* (/ x 180) pi)))

(defun invert (x) 
  (let ((string (format nil "~2,'0X" x)))
    (format nil "~A~A"
            (elt string 1) (elt string 0))))

(defvar *adr* 0) 

(defun low-nibble (x) 
  (let ((string (format nil "~2,'0X" x)))
    (format t "~2,'0X 0~A~A~%"
            (incf *adr*)
            (elt string 1)
            (elt string 1))))

(defun high-nibble (x) 
  (let ((string (format nil "~2,'0X" x)))
    (format t "~2,'0X 0~A~A~%"
            (incf *adr*)
            (elt string 0)
            (elt string 0))))

(defun low-nibble0 (x) 
  (let ((string (format nil "~2,'0X" x)))
    (format t "0~A~A~%"
            (elt string 1)
            (elt string 1))))

(defun high-nibble0 (x) 
  (let ((string (format nil "~2,'0X" x)))
    (format t "0~A~A~%"
            (elt string 0)
            (elt string 0))))


(defun test (n)
  ;; polygon k-m 
  (setf *adr* 0) 
    (loop as a from 0 by (/ 360 n) do
        (when (>= a 360)
          (return))
        (format t "~2,'0X F10~%" *adr*)
        (format t "~2,'0X 509~%" (incf *adr*))
        (low-nibble (round (+ #x40 (* #x3f (cos-d a)))))
        (high-nibble (round (+ #x40 (* #x3f (cos-d a)))))
        (low-nibble (round (+ #x10 (* #x0f (sin-d a)))))
        (high-nibble (round (+ #x10 (* #x0f (sin-d a)))))
        (format t "~2,'0X 50B~%" (incf *adr*))
        (loop as b from 0 by (/ 360 n) do
              (when (>= b 360)
                (return))
              (low-nibble (round (+ #x40 (* #x3f (cos-d b)))))
              (high-nibble (round (+ #x40 (* #x3f (cos-d b)))))
              (low-nibble (round (+ #x10 (* #x0f (sin-d b)))))
              (high-nibble (round (+ #x10 (* #x0f (sin-d b))))))))

(defun test2 (n)
  ;; circle granularity n 
  (let ((rx #x3f)
        (ry #x0f))
    (format t "F10~%")
    (format t "509~%")
    (loop as a from 0 by (/ 360 n) do
          (when (> a (+ 360 (/ 360 n)))
            (return))
          (low-nibble0 (round (+ #x40 (* rx (cos-d a)))))
          (high-nibble0 (round (+ #x40 (* rx (cos-d a)))))
          (low-nibble0 (round (+ #x10 (* ry (sin-d a)))))
          (high-nibble0 (round (+ #x10 (* ry (sin-d a)))))
          (when (> a 0) 
            (format t "50C~%")))
    (format t "F00~%")))

(defun test3 (n m) 
  ;; with sound! and nested circles 
  ;; not good... 
  (let* ((rx #x3f)
         (ry #x0f)
         (ix (round (/ rx m)))
         (iy (round (/ ry m))))
    (format t "F10~%")
    (dotimes (i m) 
      (loop as a from 0 by (/ 360 n) 
            as step from 0 by 1 do
            (format t "50D~%")
            (low-nibble0 i)
            (low-nibble0 step)      
            (format t "509~%")
            (when (> a (+ 360 (/ 360 n)))
              (return))
            (low-nibble0 (round (+ #x40 (* rx (cos-d a)))))
            (high-nibble0 (round (+ #x40 (* rx (cos-d a)))))
            (low-nibble0 (round (+ #x10 (* ry (sin-d a)))))
            (high-nibble0 (round (+ #x10 (* ry (sin-d a)))))
            (when (> a 0) 
              (format t "50C~%")))
      (decf rx ix)
      (decf ry iy))
    (format t "502~%")
    (format t "C00~%")))

(defun test3a (n m) 
  ;; with sound! and nested circles 
  (let* ((rx #x3f)
         (ry #x0f)
         (ix (round (/ rx m)))
         (iy (round (/ ry m))))
    (format t "F10~%")
    (dotimes (i m) 
      (format t "50D~%")
      (low-nibble0 1)
      (low-nibble0 i)      
      (format t "509~%")            
      (loop as a from 0 by (/ 360 n) 
            as step from 0 by 1 do
            (when (> a (+ 360 (/ 360 n)))
              (return))
            (low-nibble0 (round (+ #x40 (* rx (cos-d a)))))
            (high-nibble0 (round (+ #x40 (* rx (cos-d a)))))
            (low-nibble0 (round (+ #x10 (* ry (sin-d a)))))
            (high-nibble0 (round (+ #x10 (* ry (sin-d a)))))
            (when (> a 0) 
              (format t "50C~%")))
      (decf rx ix)
      (decf ry iy))
    (format t "502~%")
    (format t "C00~%")))

(defun test4 (n m)
  ;; nested circles animation 
  (let* ((rx #x3f)
         (ry #x0f)
         (ix (round (/ rx m)))
         (iy (round (/ ry m))))
    (format t "F10~%")
    (format t "503~%")
    (dotimes (i m) 
      (format t "509~%")
      (loop as a from 0 by (/ 360 n) do
            (when (> a (+ 360 (/ 360 n)))
              (return))
            (low-nibble0 (round (+ #x40 (* rx (cos-d a)))))
            (high-nibble0 (round (+ #x40 (* rx (cos-d a)))))
            (low-nibble0 (round (+ #x10 (* ry (sin-d a)))))
            (high-nibble0 (round (+ #x10 (* ry (sin-d a)))))
            (when (> a 0) 
              (format t "50C~%")))
      (format t "504~%")
      (decf rx ix)
      (decf ry iy))
    (format t "502~%")
    (format t "503~%")
    (format t "C00~%")))

(defun test5 (n m step)
  ;; nested circle animation with step rotation for each  
  (let* ((rx #x3f)
         (ry #x0f)
         (ix (round (/ rx m)))
         (iy (round (/ ry m)))
         (aoff 0))
    (format t "F10~%")
    (format t "503~%")
    (dotimes (i m) 
      (format t "509~%")
      (loop as a from 0 by (/ 360 n) do
            (when (> a (+ 360 (/ 360 n)))
              (return))
            (let ((a (+ a aoff)))
              (low-nibble0 (round (+ #x40 (* rx (cos-d a)))))
              (high-nibble0 (round (+ #x40 (* rx (cos-d a)))))
              (low-nibble0 (round (+ #x10 (* ry (sin-d a)))))
              (high-nibble0 (round (+ #x10 (* ry (sin-d a))))))
            (when (> a 0) 
              (format t "50C~%")))
      (format t "504~%")
      (decf rx ix)
      (decf ry iy)
      (incf aoff step))
    (format t "502~%")
    (format t "503~%")
    (format t "C00~%")))

(defun string-to-ascii (string file)
  (format file  "F10")
  (format file  "~%502")
  (format file  "~%50E")
  (format file  "~%506")

  (dolist (x ;;(substitute #\Space #\Newline (coerce string 'list)))
             (remove #\Newline (coerce string 'list)))
    (let ((string (string-upcase (format nil "~2,'0X" (char-code x)))))
      (format file  "~%0~A~A" (elt string 1)  (elt string 1))
      (format file  "~%0~A~A" (elt string 0)  (elt string 0)))))


(defun page-bank-reader (pages) 
  (let ((bank 0))
    (dolist (page pages) 
      (with-open-file (file
                       (format nil "c:/temp/PAGE~A.MIC" bank)
                       :direction :output
                       :if-exists :supersede)
        (string-to-ascii page file)
        (incf bank)
        (format file "~%50F")
        (format file "~%0AA")
        (format file "~%000")
        (format file "~%FF0")
        (format file "~%70~X" bank)
        (format file "~%C00~%~%")))))

(page-bank-reader (list "Hi. Nice to meet
you.  I  am  the
Microtronic 2090
Computer System."

"I was created in
1981 by the  ELO
magazine and the
company   Busch."

"My CPU is a 4bit
T M S 1600 micro
controller.  And
I have  a RAM of"

"256 12bit words.
I speak a custom
machine language
and I am big  in"

"education.  With
my PicoRAM I can
now  do   things
that ought to be"

"impossible   for
me.  My   master
LambaMikel loves
me. Bye bye now."))




(defun spiral (n m ) 
  ;; nested circle animation with step rotation for each  
  (let* ((rx #x3f)
         (ry #x0f)
         (irx (/ rx (* n m)))
         (iry (/ ry (* n m))))
    (format t "F10~%")
    (format t "509~%")
    (loop as a from 0 by (/ 360 n) 
          as step from 1 to (* n m) do 
          (low-nibble0 (round (+ #x40 (* (round rx) (cos-d a)))))
          (high-nibble0 (round (+ #x40 (* (round rx) (cos-d a)))))
          (low-nibble0 (round (+ #x10 (* (round ry) (sin-d a)))))
          (high-nibble0 (round (+ #x10 (* (round ry) (sin-d a)))))
          (when (> a 0) 
            (format t "50C~%"))
          (decf rx irx)
          (decf ry iry))

    (format t "502~%")
    (format t "C00~%")))