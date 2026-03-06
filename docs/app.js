/* ═══ Glasspectrum — Landing Page JS ═══ */

// ── Preset database ─────────────────────────────────────
const presets = [
    // Cinema Primes
    { name: "Master Prime 35mm", cat: "primes", era: "Modern" },
    { name: "Master Prime 50mm", cat: "primes", era: "Modern" },
    { name: "Master Prime 75mm", cat: "primes", era: "Modern" },
    { name: "Ultra Prime 24mm", cat: "primes", era: "Modern" },
    { name: "Ultra Prime 85mm", cat: "primes", era: "Modern" },
    { name: "Signature Prime 40mm", cat: "primes", era: "Modern" },
    { name: "Signature Prime 75mm", cat: "primes", era: "Modern" },
    { name: "S4/i 27mm", cat: "primes", era: "Modern" },
    { name: "S4/i 50mm", cat: "primes", era: "Modern" },
    { name: "S4/i 100mm", cat: "primes", era: "Modern" },
    { name: "Cooke S7/i 50mm", cat: "primes", era: "Modern" },
    { name: "Canon K-35 35mm", cat: "primes", era: "Modern" },
    { name: "Canon K-35 85mm", cat: "primes", era: "Modern" },
    // Cinema Zooms
    { name: "Angénieux Optimo 24-290", cat: "zooms", era: "Modern" },
    { name: "Angénieux EZ-1 30-90", cat: "zooms", era: "Modern" },
    { name: "Fujinon Premista 28-100", cat: "zooms", era: "Modern" },
    { name: "Fujinon Cabrio 19-90", cat: "zooms", era: "Modern" },
    { name: "Cooke Varotal 18-100", cat: "zooms", era: "Vintage" },
    { name: "Canon CN-E 30-300", cat: "zooms", era: "Modern" },
    { name: "Zeiss 70-200 CZ.2", cat: "zooms", era: "Modern" },
    // Anamorphic
    { name: "Master Anamorphic 50", cat: "anamorphic", era: "Modern", ana: true },
    { name: "Master Anamorphic 75", cat: "anamorphic", era: "Modern", ana: true },
    { name: "Master Anamorphic 100", cat: "anamorphic", era: "Modern", ana: true },
    { name: "Hawk V-Lite 50", cat: "anamorphic", era: "Modern", ana: true },
    { name: "Hawk V-Plus 75", cat: "anamorphic", era: "Modern", ana: true },
    { name: "Panavision C-Series", cat: "anamorphic", era: "Vintage", ana: true },
    { name: "Panavision E-Series", cat: "anamorphic", era: "Modern", ana: true },
    { name: "Panavision T-Series", cat: "anamorphic", era: "Modern", ana: true },
    { name: "Kowa Anamorphic 50", cat: "anamorphic", era: "Vintage", ana: true },
    { name: "LOMO Anamorphic Sq.", cat: "anamorphic", era: "Vintage", ana: true },
    // Vintage & Character
    { name: "Cooke Panchro 50mm", cat: "vintage", era: "Vintage" },
    { name: "Cooke Speed Panchro", cat: "vintage", era: "Classic" },
    { name: "Helios 44-2 58mm", cat: "vintage", era: "Soviet" },
    { name: "Super Takumar 50mm", cat: "vintage", era: "Classic" },
    { name: "Canon FD 50mm f/1.4", cat: "vintage", era: "Classic" },
    { name: "Nikkor AI-S 50mm", cat: "vintage", era: "Classic" },
    { name: "Minolta MD 50mm", cat: "vintage", era: "Classic" },
    { name: "Leica Summilux-C 50", cat: "vintage", era: "Modern" },
    { name: "Leica Summicron-C 40", cat: "vintage", era: "Modern" },
    { name: "Bausch+Lomb Super Baltar", cat: "vintage", era: "Classic" },
    { name: "Kinoptik Tegea 9.8mm", cat: "vintage", era: "Ultra-Wide" },
    { name: "LOMO Standard 50mm", cat: "vintage", era: "Soviet" },
    { name: "LOMO OKC 35mm", cat: "vintage", era: "Soviet" },
    // Exotic
    { name: "Petzval 85mm Art", cat: "exotic", era: "Special" },
    { name: "Lensbaby Velvet 56", cat: "exotic", era: "Creative" },
    { name: "Meyer Trioplan 100", cat: "exotic", era: "Bubble" },
    { name: "Daguerreotype Achromat", cat: "exotic", era: "Special" },
    { name: "Holga Plastic Lens", cat: "exotic", era: "Lo-Fi" },
    { name: "Pinhole f/128", cat: "exotic", era: "Minimal" },
    { name: "Lomography Atoll 17", cat: "exotic", era: "Ultra-Wide" },
];

// ── Render presets ──────────────────────────────────────
const grid = document.getElementById('presetGrid');
const tabs = document.querySelectorAll('.preset-tab');
let currentCat = 'primes';

function renderPresets(cat) {
    const filtered = presets.filter(p => p.cat === cat);
    grid.innerHTML = filtered.map(p => `
    <div class="preset-item">
      <span class="lens-name">${p.name}</span>
      ${p.ana ? '<span class="lens-badge">ANA</span>' : ''}
      <span class="lens-badge">${p.era}</span>
    </div>
  `).join('');
}

tabs.forEach(tab => {
    tab.addEventListener('click', () => {
        tabs.forEach(t => t.classList.remove('active'));
        tab.classList.add('active');
        currentCat = tab.dataset.cat;
        renderPresets(currentCat);
    });
});

renderPresets('primes');

// ── Nav scroll effect ───────────────────────────────────
const nav = document.getElementById('nav');
let ticking = false;
window.addEventListener('scroll', () => {
    if (!ticking) {
        requestAnimationFrame(() => {
            nav.classList.toggle('scrolled', window.scrollY > 50);
            ticking = false;
        });
        ticking = true;
    }
});

// ── Smooth scroll for anchor links ─────────────────────
document.querySelectorAll('a[href^="#"]').forEach(a => {
    a.addEventListener('click', e => {
        const target = document.querySelector(a.getAttribute('href'));
        if (target) {
            e.preventDefault();
            target.scrollIntoView({ behavior: 'smooth' });
        }
    });
});

// ── Intersection Observer for reveal animations ─────────
const observer = new IntersectionObserver((entries) => {
    entries.forEach(entry => {
        if (entry.isIntersecting) {
            entry.target.classList.add('revealed');
            observer.unobserve(entry.target);
        }
    });
}, { threshold: 0.1, rootMargin: '0px 0px -60px 0px' });

document.querySelectorAll('.feature-card, .step-card, .download-card, .support-card').forEach(el => {
    el.style.opacity = '0';
    el.style.transform = 'translateY(20px)';
    el.style.transition = 'opacity .5s ease, transform .5s ease';
    observer.observe(el);
});

// Reveal class
const style = document.createElement('style');
style.textContent = '.revealed { opacity: 1 !important; transform: none !important; }';
document.head.appendChild(style);

// ── Auto-detect user platform for download highlight ────
const ua = navigator.userAgent.toLowerCase();
let platform = 'mac';
if (ua.includes('win')) platform = 'win';
else if (ua.includes('linux')) platform = 'linux';

const highlightId = { mac: 'dl-mac', win: 'dl-win', linux: 'dl-linux' }[platform];
const highlightEl = document.getElementById(highlightId);
if (highlightEl) {
    highlightEl.style.borderColor = 'rgba(96,165,250,.4)';
    highlightEl.style.background = 'rgba(96,165,250,.06)';
    const badge = document.createElement('span');
    badge.textContent = 'Recommended';
    badge.style.cssText = 'position:absolute;top:-10px;right:16px;font-size:.7rem;font-weight:600;padding:3px 10px;border-radius:6px;background:linear-gradient(135deg,#60A5FA,#C084FC);color:#fff;';
    highlightEl.style.position = 'relative';
    highlightEl.appendChild(badge);
}
