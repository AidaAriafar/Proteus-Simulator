ریزرپروژه ۸ — کنترل شبیه‌سازی با SDL2

فایل‌ها:
1) Project8.cpp
2) CMakeLists.txt

امکانات:
- Run، Pause، Stop
- حفظ کامل وضعیت و رخدادهای آینده در Pause
- صفر شدن زمان و حذف رخدادها در Stop
- رنگ زنده سیم‌ها: High قرمز، Low آبی، Undefined زرد
- بازگشت رنگ سیم‌ها به خاکستری در Stop
- تعامل زنده با Toggle، Push Button و Potentiometer
- تأخیر انتشار گیت AND و ADC سه‌بیتی با صف رخداد
- Step با گام 0.10 ثانیه
- Next Event برای رفتن دقیق تا رخداد منطقی بعدی
- کلاک یک هرتز برای آزمایش اجرای پیوسته و گام‌به‌گام

کلیدها:
Space: Run/Pause
R: Stop
S: Fixed Step
N: Next Event
T: Toggle Switch
P: Push Button (تا وقتی کلید نگه داشته شده)
Left/Right: تغییر پتانسیومتر
