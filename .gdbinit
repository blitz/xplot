break main

commands 1
silent
set _Xdebug=1
cont
end

break XCloseDisplay
break exit
break panic
break fatalerror
