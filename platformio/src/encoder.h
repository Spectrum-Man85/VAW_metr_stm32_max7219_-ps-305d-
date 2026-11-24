  volatile int encoder = 0;
  volatile uint8_t prevState = 0;




  extern  int S1 ; //
  extern  int S2 ; //


  void encoderISR()
{
  uint8_t a = digitalRead(S1);
  uint8_t b = digitalRead(S2);

  uint8_t state = (a << 1) | b;
  uint8_t transition = (prevState << 2) | state;

  switch (transition)
  {
    case 0b0001:
    case 0b0111:
    case 0b1110:
    case 0b1000:
      encoder++;
      break;

    case 0b0010:
    case 0b0100:
    case 0b1101:
    case 0b1011:
      encoder--;
      break;
  }

  prevState = state;
}

