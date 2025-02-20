# Vision API Examples for LilyGo v1.6.2

This directory contains examples that integrate the **Gemini Vision API** with the **LilyGo v1.6.2** board.

## **Example Implementations**
### 1. **my_trial**
- Captures a frame every **5 seconds**.
- Sends the frame to **Gemini Vision API**.
- Prints the API response over the **serial port**.
- **Limitation**: This method is **slow** due to API rate limits, which may change over time.

### 2. **my_trial_button_based**
- Captures a frame **only when a button is pressed**.
- Sends the captured frame to **Gemini Vision API**.
- **More efficient** compared to `my_trial` as it reduces unnecessary API calls.

## **Notes**
- Ensure the **capture delay** in `my_trial` is adjusted according to **Gemini API usage limits**.
- Both examples require **network connectivity** to send frames to the API.

## **Setup & Usage**
1. Connect the **LilyGo v1.6.2** board to your system.
2. Upload the desired example (`my_trial` or `my_trial_button_based`).
3. Monitor the **serial output** to view the API response.

## **License**
This project is licensed under the MIT License - see the `LICENSE` file for details.
