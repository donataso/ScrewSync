; Cone 20x10x30
; X20 Z0
; X10 Z-30

; Run time 1.2 min, cutting 85% of time
; Working area 10.00 by 10.00 mm
; Axes expected: Z+ points right, X+ points up

G21 ; metric
G18 ; ZX plane
G90 ; absolute positioning
F100
X20 ; zero your tool X on centerline
Z0 ; zero your tool Z at the right edge of the stock
G91 ; relative positioning
Z-1.5 ; cut 0.3 mm2
Z-0.5 X-0.25 ; cut 0.1 mm2 at 63.43°

F80
X-0.1
