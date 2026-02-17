# Running logisim in command line mode (without GUI)

Logisim runs much faster in command line mode, because it does not waste time
on redrawing the GUI several times per second. This may be important if your
computer is slow. 

To start logisim in command line mode, open Terminal and cd into *this folder*, 
then from Terminal type something like:

     java -jar ~/logisim-evolution-ucd-4.0.2rc01-all.jar computer.circ -t tty

With -t tty, ASCII display and keyboard in Logisim circuit are mapped to console.

Note that *before* you run logisim without GUI you must first start logisim *with* GUI to
load your compiled code into the ROM of computer.circ and *save* the modified circuit.

