#include "stm32f1xx_hal.h"

#define INITIAL_MINUTES 1U
#define DEBOUNCE_MS 35U

#define LED_Pin GPIO_PIN_8
#define LED_GPIO_Port GPIOA

#define BTN_INCREMENT_Pin GPIO_PIN_13
#define BTN_INCREMENT_GPIO_Port GPIOC

#define BTN_START_PAUSE_Pin GPIO_PIN_14
#define BTN_START_PAUSE_GPIO_Port GPIOC

#define BTN_RESET_Pin GPIO_PIN_15
#define BTN_RESET_GPIO_Port GPIOC

#define SEG_A_Pin GPIO_PIN_0
#define SEG_A_GPIO_Port GPIOA

#define SEG_B_Pin GPIO_PIN_1
#define SEG_B_GPIO_Port GPIOA

#define SEG_C_Pin GPIO_PIN_2
#define SEG_C_GPIO_Port GPIOA

#define SEG_D_Pin GPIO_PIN_3
#define SEG_D_GPIO_Port GPIOA

#define SEG_E_Pin GPIO_PIN_4
#define SEG_E_GPIO_Port GPIOA

#define SEG_F_Pin GPIO_PIN_5
#define SEG_F_GPIO_Port GPIOA

#define SEG_G_Pin GPIO_PIN_6
#define SEG_G_GPIO_Port GPIOA

#define DIGIT_1_Pin GPIO_PIN_12
#define DIGIT_1_GPIO_Port GPIOB

#define DIGIT_2_Pin GPIO_PIN_13
#define DIGIT_2_GPIO_Port GPIOB

#define DIGIT_3_Pin GPIO_PIN_14
#define DIGIT_3_GPIO_Port GPIOB

#define DIGIT_4_Pin GPIO_PIN_15
#define DIGIT_4_GPIO_Port GPIOB

typedef enum {
  TIMER_PAUSED = 0,
  TIMER_RUNNING,
  TIMER_FINISHED
} TimerState;

typedef struct {
  GPIO_TypeDef *port;
  uint16_t pin;
  uint8_t stableState;
  uint8_t lastRawState;
  uint8_t pressedEvent;
  uint32_t lastChangeMs;
} Button;

static void SystemClock_Config(void);
static void MX_GPIO_Init(void);

static void Display_UpdateValue(uint16_t totalSeconds);
static void Display_Multiplex(void);
static void Display_TurnOffDigits(void);
static void Display_SelectDigit(uint8_t digitIndex);

static void Led_On(void);
static void Led_Off(void);

static uint8_t Button_ReadRaw(Button *button);
static void Button_Update(Button *button);
static uint8_t Button_WasPressed(Button *button);

static void Timer_Reset(void);
static void Timer_IncrementMinutes(void);
static void Timer_ToggleStartPause(void);
static void Timer_TickOneSecond(void);

static volatile uint32_t systemMs = 0;
static volatile uint8_t oneSecondTicks = 0;

static volatile uint8_t displayDigits[4] = { 0, 1, 0, 0 };
static volatile uint8_t activeDigit = 0;

static TimerState timerState = TIMER_PAUSED;

static uint8_t configuredMinutes = INITIAL_MINUTES;
static uint16_t currentSeconds = INITIAL_MINUTES * 60U;

static Button incrementButton = {
  BTN_INCREMENT_GPIO_Port,
  BTN_INCREMENT_Pin,
  0,
  0,
  0,
  0
};

static Button startPauseButton = {
  BTN_START_PAUSE_GPIO_Port,
  BTN_START_PAUSE_Pin,
  0,
  0,
  0,
  0
};

static Button resetButton = {
  BTN_RESET_GPIO_Port,
  BTN_RESET_Pin,
  0,
  0,
  0,
  0
};

static GPIO_TypeDef *segmentPorts[7] = {
  SEG_A_GPIO_Port,
  SEG_B_GPIO_Port,
  SEG_C_GPIO_Port,
  SEG_D_GPIO_Port,
  SEG_E_GPIO_Port,
  SEG_F_GPIO_Port,
  SEG_G_GPIO_Port
};

static const uint16_t segmentPins[7] = {
  SEG_A_Pin,
  SEG_B_Pin,
  SEG_C_Pin,
  SEG_D_Pin,
  SEG_E_Pin,
  SEG_F_Pin,
  SEG_G_Pin
};

static GPIO_TypeDef *digitPorts[4] = {
  DIGIT_1_GPIO_Port,
  DIGIT_2_GPIO_Port,
  DIGIT_3_GPIO_Port,
  DIGIT_4_GPIO_Port
};

static const uint16_t digitPins[4] = {
  DIGIT_1_Pin,
  DIGIT_2_Pin,
  DIGIT_3_Pin,
  DIGIT_4_Pin
};

static const uint8_t numbers[10][7] = {
  { 1, 1, 1, 1, 1, 1, 0 },
  { 0, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 0, 1, 1, 0, 1 },
  { 1, 1, 1, 1, 0, 0, 1 },
  { 0, 1, 1, 0, 0, 1, 1 },
  { 1, 0, 1, 1, 0, 1, 1 },
  { 1, 0, 1, 1, 1, 1, 1 },
  { 1, 1, 1, 0, 0, 0, 0 },
  { 1, 1, 1, 1, 1, 1, 1 },
  { 1, 1, 1, 1, 0, 1, 1 }
};

int main(void)
{
  HAL_Init();

  SystemClock_Config();

  MX_GPIO_Init();

  Timer_Reset();

  while (1) {
    Button_Update(&incrementButton);
    Button_Update(&startPauseButton);
    Button_Update(&resetButton);

    if (Button_WasPressed(&incrementButton)) {
      Timer_IncrementMinutes();
    }

    if (Button_WasPressed(&startPauseButton)) {
      Timer_ToggleStartPause();
    }

    if (Button_WasPressed(&resetButton)) {
      Timer_Reset();
    }

    while (oneSecondTicks > 0U) {
      __disable_irq();
      oneSecondTicks--;
      __enable_irq();

      Timer_TickOneSecond();
    }
  }
}

void SysTick_Handler(void)
{
  static uint16_t msCounter = 0;

  HAL_IncTick();

  systemMs++;

  Display_Multiplex();

  msCounter++;

  if (msCounter >= 1000U) {
    msCounter = 0;
    oneSecondTicks++;
  }
}

static void SystemClock_Config(void)
{
}

static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = { 0 };

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  GPIO_InitStruct.Pin = SEG_A_Pin | SEG_B_Pin | SEG_C_Pin | SEG_D_Pin | SEG_E_Pin | SEG_F_Pin | SEG_G_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = LED_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LED_GPIO_Port, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = DIGIT_1_Pin | DIGIT_2_Pin | DIGIT_3_Pin | DIGIT_4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  GPIO_InitStruct.Pin = BTN_INCREMENT_Pin | BTN_START_PAUSE_Pin | BTN_RESET_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  Display_TurnOffDigits();
  Led_Off();
}

static void Display_UpdateValue(uint16_t totalSeconds)
{
  uint8_t minutes = totalSeconds / 60U;
  uint8_t seconds = totalSeconds % 60U;

  displayDigits[0] = minutes / 10U;
  displayDigits[1] = minutes % 10U;
  displayDigits[2] = seconds / 10U;
  displayDigits[3] = seconds % 10U;
}

static void Display_Multiplex(void)
{
  uint8_t segment = 0;

  Display_TurnOffDigits();

  for (segment = 0; segment < 7U; segment++) {
    GPIO_PinState state = GPIO_PIN_RESET;

    if (numbers[displayDigits[activeDigit]][segment] == 1U) {
      state = GPIO_PIN_SET;
    }

    HAL_GPIO_WritePin(segmentPorts[segment], segmentPins[segment], state);
  }

  Display_SelectDigit(activeDigit);

  activeDigit++;

  if (activeDigit >= 4U) {
    activeDigit = 0;
  }
}

static void Display_TurnOffDigits(void)
{
  uint8_t index = 0;

  for (index = 0; index < 4U; index++) {
    HAL_GPIO_WritePin(digitPorts[index], digitPins[index], GPIO_PIN_SET);
  }
}

static void Display_SelectDigit(uint8_t digitIndex)
{
  HAL_GPIO_WritePin(digitPorts[digitIndex], digitPins[digitIndex], GPIO_PIN_RESET);
}

static void Led_On(void)
{
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET);
}

static void Led_Off(void)
{
  HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET);
}

static uint8_t Button_ReadRaw(Button *button)
{
  if (HAL_GPIO_ReadPin(button->port, button->pin) == GPIO_PIN_RESET) {
    return 1U;
  }

  return 0U;
}

static void Button_Update(Button *button)
{
  uint8_t rawState = Button_ReadRaw(button);

  if (rawState != button->lastRawState) {
    button->lastRawState = rawState;
    button->lastChangeMs = systemMs;
  }

  if ((systemMs - button->lastChangeMs) < DEBOUNCE_MS) {
    return;
  }

  if (rawState == button->stableState) {
    return;
  }

  button->stableState = rawState;

  if (button->stableState == 1U) {
    button->pressedEvent = 1U;
  }
}

static uint8_t Button_WasPressed(Button *button)
{
  if (button->pressedEvent == 0U) {
    return 0U;
  }

  button->pressedEvent = 0U;

  return 1U;
}

static void Timer_Reset(void)
{
  timerState = TIMER_PAUSED;
  currentSeconds = configuredMinutes * 60U;

  Led_Off();
  Display_UpdateValue(currentSeconds);
}

static void Timer_IncrementMinutes(void)
{
  if (timerState == TIMER_RUNNING) {
    return;
  }

  configuredMinutes++;

  if (configuredMinutes > 99U) {
    configuredMinutes = 1U;
  }

  Timer_Reset();
}

static void Timer_ToggleStartPause(void)
{
  if (timerState == TIMER_FINISHED) {
    Timer_Reset();
    return;
  }

  if (timerState == TIMER_RUNNING) {
    timerState = TIMER_PAUSED;
    return;
  }

  timerState = TIMER_RUNNING;
  Led_Off();
}

static void Timer_TickOneSecond(void)
{
  if (timerState != TIMER_RUNNING) {
    return;
  }

  if (currentSeconds > 0U) {
    currentSeconds--;
    Display_UpdateValue(currentSeconds);
  }

  if (currentSeconds == 0U) {
    timerState = TIMER_FINISHED;
    Led_On();
  }
}
