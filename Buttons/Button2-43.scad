// Button for shield 4-key

$fn=30;          // precision of rendering

$round=0.7;      // rounding corners
$size=5.6;       // edge size
$high=3.0;       // height 
$frame=1.4;      // lower frame (bottom width is $size + $frame)
$frameHigh=0.6;  // height frame
//$holeHigh=2.5; 
//$hole=2.3;     


difference() {
    union() {
        translate([0,0,$high/2])
            cube([$size-2*$round,$size,$high],true);
        translate([0,0,$high/2])
            cube([$size,$size-2*$round,$high],true);
        translate([0,0,$high])
            cube([$size-2*$round,$size-2*$round,2*$round],true);
        // rounded horizontal edges
        translate([0,$size/2-$round,$high]) rotate(90,[0,1,0]) 
            cylinder($size-2*$round,$round,$round, center=true); 
        translate([0,-$size/2+$round,$high]) rotate(90,[0,1,0]) 
            cylinder($size-2*$round,$round,$round, center=true); 
        translate([$size/2-$round,0,$high]) rotate(90,[1,0,0]) 
            cylinder($size-2*$round,$round,$round, center=true); 
        translate([-$size/2+$round,0,$high]) rotate(90,[1,0,0]) 
            cylinder($size-2*$round,$round,$round, center=true); 
        // rounded corners
        translate([-$size/2+$round,-$size/2+$round,$high/2]) 
            cylinder($high,$round,$round,true);
        translate([-$size/2+$round,-$size/2+$round,$high]) 
            sphere($round); 
        translate([-$size/2+$round,$size/2-$round,$high/2]) 
            cylinder($high,$round,$round, center=true); 
        translate([-$size/2+$round,$size/2-$round,$high]) 
            sphere($round); 
        translate([$size/2-$round,-$size/2+$round,$high/2]) 
            cylinder($high,$round,$round, center=true); 
        translate([$size/2-$round,-$size/2+$round,$high]) 
            sphere($round); 
        translate([$size/2-$round,$size/2-$round,$high/2]) 
            cylinder($high,$round,$round, center=true); 
        translate([$size/2-$round,$size/2-$round,$high]) 
            sphere($round); 
        // lower frame
        translate([0,0,$frameHigh/2])
            cube([$size+$frame,$size,$frameHigh],true);    
        translate([0,0,$frameHigh/2])
            cube([$size,$size+$frame,$frameHigh],true);    
        translate([-($size+$frame)/2+$frame/2,-($size+$frame)/2+$frame/2,$frameHigh/2]) 
            cylinder($frameHigh,$frame/2,$frame/2,true);            
        translate([-($size+$frame)/2+$frame/2,+($size+$frame)/2-$frame/2,$frameHigh/2]) 
            cylinder($frameHigh,$frame/2,$frame/2,true);            
        translate([+($size+$frame)/2-$frame/2,-($size+$frame)/2+$frame/2,$frameHigh/2]) 
            cylinder($frameHigh,$frame/2,$frame/2,true);            
        translate([+($size+$frame)/2-$frame/2,+($size+$frame)/2-$frame/2,$frameHigh/2]) 
            cylinder($frameHigh,$frame/2,$frame/2,true);            
    };

    translate([0,0,$high+0.5])
        linear_extrude(1) polygon(points=[[-1.5,0],[1.5,1.5],[1.5,-1.5]]);
};


