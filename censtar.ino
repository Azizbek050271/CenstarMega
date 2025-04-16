// censtar.ino  ──────────────────────────────────────────────────────────
@@
 #include "rs422.h"
 
 static FSMContext fsmContext;
+static unsigned long welcomeUntil = 0;   // ← добавлено
+static bool welcomeShown = false;        // ← добавлено
 
 void setup() {
-    Serial.begin(9600);
+    Serial.begin(9600);                  // можно 115200, но оставляем как было
     initOLED();
     initRS422();
     initFSM(&fsmContext);
     displayMessage("CENSTAR");
-    unsigned long welcomeStart = millis();
-    while (millis() - welcomeStart < DISPLAY_WELCOME_DURATION) {
-    }
+    welcomeUntil = millis() + DISPLAY_WELCOME_DURATION;  // ← новая логика
+    welcomeShown = true;
 }
 
 void loop() {
+    // 1. Неблокирующее завершение приветствия
+    if (welcomeShown && millis() >= welcomeUntil) {
+        welcomeShown = false;
+        if (fsmContext.priceValid) {
+            displayMessage("Ready");
+        }
+    }
+
     char key = getKeypadKey();
     if (key) {
         processKeyFSM(&fsmContext, key);
     }
     updateFSM(&fsmContext);
+    // yield();  // (необязательно, но можно добавить для дружбы с Wi‑Fi/USB)
 }
