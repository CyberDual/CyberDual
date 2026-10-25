const variants = new Array(100).fill("fonipa").join("-");

try {
  new Intl.Locale("en", { variants });
} catch {}
