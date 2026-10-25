const variants = Array.from({length: 48}, (_, index) => `aaaa${index.toString(36)}`).join("-");
new Intl.Locale("en", { variants });
