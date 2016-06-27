// Color conversion routines
// based on http://www.developers.meethue.com/documentation/color-conversions-rgb-xy

void rgb2xy(float r, float g, float b, float *x, float *y) {
  // Apply gamma correction
  float red = (red > 0.04045f) ? pow((red + 0.055f) / (1.0f + 0.055f), 2.4f) : (red / 12.92f);
  float green = (green > 0.04045f) ? pow((green + 0.055f) / (1.0f + 0.055f), 2.4f) : (green / 12.92f);
  float blue = (blue > 0.04045f) ? pow((blue + 0.055f) / (1.0f + 0.055f), 2.4f) : (blue / 12.92f);

  // Convert the RGB values to XYZ using the Wide RGB D65 conversion formula
  float X = red * 0.664511f + green * 0.154324f + blue * 0.162028f;
  float Y = red * 0.283881f + green * 0.668433f + blue * 0.047685f;
  float Z = red * 0.000088f + green * 0.072310f + blue * 0.986039f;

  // Calculate the xy values from the XYZ values
  *x = X / (X + Y + Z);
  *y = Y / (X + Y + Z);
}

void xy2rgb(float x, float y, float brightness, float *r, float *g, float *b) {

  // Calculate XYZ values
  float z = 1.0f - x - y;
  float Y = brightness; // The given brightness value
  float X = (Y / y) * x;
  float Z = (Y / y) * z;

  // Convert to RGB using Wide RGB D65 conversion
  *r =  X * 1.656492f - Y * 0.354851f - Z * 0.255038f;
  *g = -X * 0.707196f + Y * 1.655397f + Z * 0.036152f;
  *b =  X * 0.051713f - Y * 0.121364f + Z * 1.011530f;


 // Apply reverse gamma correction
  *r = *r <= 0.0031308f ? 12.92f * *r : (1.0f + 0.055f) * pow(*r, (1.0f / 2.4f)) - 0.055f;
  *g = *g <= 0.0031308f ? 12.92f * *g : (1.0f + 0.055f) * pow(*g, (1.0f / 2.4f)) - 0.055f;
  *b = *b <= 0.0031308f ? 12.92f * *b : (1.0f + 0.055f) * pow(*b, (1.0f / 2.4f)) - 0.055f;
}

