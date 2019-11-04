;;; Directory Local Variables
;;; For more information see (info "(emacs) Directory Variables")

((nil
  (fill-column . 90)
  (eval . (condition-case nil
              (progn
                (add-to-list 'grep-find-ignored-files "*TAGS" )
                (add-to-list 'grep-find-ignored-files "*cpp.py" ))
            (error nil)))
  (eval . (condition-case nil
              (progn
                (ws-butler-mode))
            (error nil)))
  (eval . (c-set-offset 'innamespace 0))))
