// Applies the stored theme before first paint (kept external to satisfy the CSP).
(function () {
  try {
    var t = localStorage.getItem('cml.theme');
    if (t === 'light' || t === 'dark') document.documentElement.setAttribute('data-theme', t);
  } catch (e) {
    /* storage unavailable */
  }
})();
