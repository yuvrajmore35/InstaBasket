#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <MFRC522.h>

// --- WIFI CONFIGURATION ---
const char* ssid = "vivo Y75 5G"; 
const char* password = "sat@1237";

#define SS_PIN 5
#define RST_PIN 0
#define HOLD_TIME 2000

WebServer server(80);
MFRC522 rfid(SS_PIN, RST_PIN);

// Item database for RFID Hardware
struct Item {
  byte uid[4];
  String name;
  float price;
};

Item items[] = {
  {{0x13, 0x5A, 0x2B, 0x1C}, "Maggie", 25.00},
  {{0xA3, 0x5B, 0x2C, 0x1D}, "Hide and Seek", 35.00},
  {{0xB4, 0x6C, 0x3D, 0x2E}, "Oats", 15.00},
  {{0xC5, 0x7D, 0x4E, 0x3F}, "Books", 45.00},
  {{0xD6, 0x8E, 0x5F, 0x40}, "Headphones", 55.00},
  {{0xE7, 0x9F, 0x60, 0x51}, "Wireless Mouse", 75.00},
  {{0xF8, 0xA0, 0x71, 0x62}, "USB Hub", 20.00},
  {{0x09, 0xB1, 0x82, 0x73}, "SD Card 64GB", 18.00}
};
const int itemCount = 8;

struct CartItem {
  byte uid[4];
  String name;
  float price;
  int quantity;
};
CartItem cart[10];
int cartCount = 0;

byte lastCardUID[4] = {0, 0, 0, 0};
unsigned long cardDetectedTime = 0;
bool cardHeld = false;

// --- Helper Functions ---
bool compareUID(byte* uid1, byte* uid2) {
  for (int i = 0; i < 4; i++) if (uid1[i] != uid2[i]) return false;
  return true;
}

void copyUID(byte* dest, byte* src) {
  for (int i = 0; i < 4; i++) dest[i] = src[i];
}

int getItemIndexFromUID(byte* uid) {
  int sum = 0;
  for (int i = 0; i < 4; i++) sum += uid[i];
  return sum % itemCount;
}

void addToCart(byte* uid, String name, float price) {
  for (int i = 0; i < cartCount; i++) {
    if (compareUID(cart[i].uid, uid)) {
      cart[i].quantity++;
      return;
    }
  }
  if (cartCount < 10) {
    copyUID(cart[cartCount].uid, uid);
    cart[cartCount].name = name;
    cart[cartCount].price = price;
    cart[cartCount].quantity = 1;
    cartCount++;
  }
}

void removeFromCart(byte* uid) {
  for (int i = 0; i < cartCount; i++) {
    if (compareUID(cart[i].uid, uid)) {
      if (cart[i].quantity > 1) { cart[i].quantity--; } 
      else {
        for (int j = i; j < cartCount - 1; j++) cart[j] = cart[j + 1];
        cartCount--;
      }
      return;
    }
  }
}

// --- ENHANCED DARK THEME HTML ---
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>Instabasket | Smart System</title>
  <style>
    :root {
      --bg: #0a0a0c;
      --card-bg: #16161a;
      --accent: #3b82f6;
      --accent-glow: rgba(59, 130, 246, 0.5);
      --text-p: #ffffff;
      --text-s: #94a3b8;
      --danger: #ef4444;
      --success: #10b981;
    }
    * { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', Roboto, Helvetica, Arial, sans-serif; }
    body { background: var(--bg); color: var(--text-p); overflow-x: hidden; padding-bottom: 60px; }
    .container { max-width: 450px; margin: 0 auto; background: var(--bg); min-height: 100vh; display: none; position: relative; border-left: 1px solid #222; border-right: 1px solid #222; }
    .active { display: block; animation: fadeIn 0.5s ease; }
    @keyframes fadeIn { from { opacity: 0; transform: translateY(10px); } to { opacity: 1; transform: translateY(0); } }
    header { padding: 30px 20px; display: flex; align-items: center; justify-content: space-between; background: rgba(10, 10, 12, 0.9); backdrop-filter: blur(10px); position: sticky; top: 0; z-index: 100; }
    .logo { font-size: 32px; font-weight: 800; letter-spacing: -1.5px; background: linear-gradient(90deg, #3b82f6, #60a5fa); -webkit-background-clip: text; -webkit-text-fill-color: transparent; }
    .hero { padding: 60px 20px; text-align: center; }
    .hero h1 { font-size: 90px; margin-bottom: 20px; display: inline-block; animation: float 3s ease-in-out infinite; }
    @keyframes float { 0%, 100% { transform: translateY(0px); } 50% { transform: translateY(-15px); } }
    .hero h2 { font-size: 36px; font-weight: 700; margin-bottom: 8px; }
    .hero p.tagline { color: var(--accent); font-weight: 600; font-size: 14px; text-transform: uppercase; letter-spacing: 1.5px; }
    .slider-container { margin: 15px 20px; height: 200px; overflow: hidden; border-radius: 15px; position: relative; border: 1px solid #333; }
    .slider { display: flex; width: 400%; height: 100%; transition: transform 0.8s ease-in-out; }
    .slide { width: 25%; height: 100%; background-size: cover; background-position: center; display: flex; align-items: flex-end; padding: 15px; }
    .slide-label { background: rgba(0,0,0,0.7); padding: 4px 10px; border-radius: 5px; font-size: 12px; font-weight: bold; color: #3b82f6; }
    .btn-box { display: flex; flex-direction: column; gap: 15px; padding: 25px; }
    .btn { padding: 18px; border: none; border-radius: 12px; font-size: 17px; font-weight: 600; cursor: pointer; transition: all 0.3s; text-align: center; }
    .btn-p { background: var(--accent); color: white; box-shadow: 0 4px 15px var(--accent-glow); }
    .btn-p:hover { transform: scale(1.02); box-shadow: 0 6px 20px var(--accent-glow); }
    .btn-s { background: #222; color: var(--text-s); }
    .btn-s:hover { background: #2d2d35; color: white; }
    .btn-outline { border: 1px solid #333; color: var(--text-p); background: transparent; padding: 8px 15px; font-size: 13px; }
    .btn-test { padding: 8px; border-radius: 8px; font-size: 12px; background: #333; color: #888; border: 1px dashed #555; cursor: pointer; margin-top: 10px; }
    .input-group { margin-bottom: 20px; padding: 0 20px; }
    label { display: block; margin-bottom: 8px; font-size: 13px; color: var(--text-s); text-transform: uppercase; letter-spacing: 0.5px; }
    input, select, textarea { width: 100%; padding: 14px; border: 1px solid #333; border-radius: 10px; font-size: 16px; background: #111; color: white; }
    .stat-bar { background: linear-gradient(135deg, #1e1e24 0%, #111 100%); margin: 15px 20px; padding: 25px; border-radius: 20px; display: flex; justify-content: space-around; text-align: center; border: 1px solid #222; }
    .stat-bar p { font-size: 11px; color: var(--text-s); margin-bottom: 5px; }
    .stat-bar h2 { font-size: 28px; color: var(--accent); }
    .card { display: flex; justify-content: space-between; align-items: center; padding: 18px; background: var(--card-bg); border-radius: 15px; margin: 0 20px 12px 20px; border: 1px solid #222; }
    .footer-mark { width: 100%; text-align: center; padding: 20px; color: #444; font-size: 14px; letter-spacing: 1px; font-weight: 500; }
    .qr-img { width: 250px; height: 250px; margin: 25px auto; display: block; border-radius: 15px; border: 8px solid white; }
    .loc-tag { font-size: 12px; color: var(--accent); font-weight: bold; }
    .store-map-img { width: calc(100% - 40px); margin: 20px; border-radius: 15px; border: 1px solid #333; display: block; }
    .floor-box { border: 1px solid #333; padding: 15px; border-radius: 12px; margin-bottom: 15px; background: #111; }
  </style>
</head>
<body>

  <div id="p-land" class="container active">
    <header><div class="logo">Instabasket</div></header>
    <div class="hero">
      <h1 id="hero-emoji">🛒</h1>
      <h2>Welcome</h2>
      <p class="tagline">The future of queue free shopping system</p>
    </div>
    <div class="btn-box">
      <button class="btn btn-p" onclick="nav('p-cust-login')">Customer Login</button>
      <button class="btn btn-s" onclick="nav('p-admin-login')">Admin Management</button>
    </div>
    <div class="footer-mark">@instabasket.in</div>
  </div>

  <template id="ad-slider-tmpl">
    <div class="slider-container">
      <div class="slider" id="mainSlider">
        <div class="slide" style="background-image: linear-gradient(to top, rgba(0,0,0,0.8), transparent), url('https://images.unsplash.com/photo-1505740420928-5e560c06d30e?w=500&q=80')">
          <span class="slide-label">40% OFF HEADPHONES</span>
        </div>
        <div class="slide" style="background-image: linear-gradient(to top, rgba(0,0,0,0.8), transparent), url('https://images.unsplash.com/photo-1591336373307-e97732411aa4?w=500&q=80')">
          <span class="slide-label">BUY 1 GET 1 SOAPS</span>
        </div>
        <div class="slide" style="background-image: linear-gradient(to top, rgba(0,0,0,0.8), transparent), url('https://images.unsplash.com/photo-1584946335122-39121f95a0fb?w=500&q=80')">
          <span class="slide-label">KITCHEN ESSENTIALS SALE</span>
        </div>
        <div class="slide" style="background-image: linear-gradient(to top, rgba(0,0,0,0.8), transparent), url('https://images.unsplash.com/photo-1542838132-92c53300491e?w=500&q=80')">
          <span class="slide-label">FLAT 20% OFF GROCERIES</span>
        </div>
      </div>
    </div>
  </template>

  <div id="p-cust-login" class="container">
    <header>
      <button style="background:none; border:none; color:white; font-size:24px;" onclick="nav('p-land')">←</button>
      <div class="logo">Check-in</div>
    </header>
    <div class="ad-placeholder"></div>
    <div style="padding-top: 10px;">
      <div class="input-group"><label>Mart ID</label><input type="text" id="targetMartId" placeholder="MART-XXXX"></div>
      <div class="btn-box"><button class="btn btn-p" onclick="customerLogin()">Initialize Basket</button></div>
    </div>
    <div class="footer-mark">@instabasket.in</div>
  </div>

  <div id="p-shopping-list" class="container">
    <header><div class="logo">Instabasket</div></header>
    <div class="ad-placeholder"></div>
    <div style="padding: 20px;">
      <h3 style="margin-bottom:10px; font-size:22px;">Smart Find</h3>
      <textarea id="userListInput" rows="4" placeholder="Apple, Milk, T-Shirt, Laptop..."></textarea>
      <div class="btn-box" style="padding: 20px 0;">
        <button class="btn btn-p" onclick="processList()">Locate Items</button>
        <button class="btn btn-s" onclick="showFloorPlan(true)">View Full Map</button>
      </div>
    </div>
    <div class="footer-mark">@instabasket.in</div>
  </div>

  <div id="p-store-guide" class="container">
    <header>
      <div class="logo">Navigator</div>
      <button id="closeMapBtn" class="btn-outline" style="display:none;" onclick="nav('p-cart')">Back</button>
    </header>
    <div class="ad-placeholder"></div>
    <div id="guideArea" style="padding: 20px;"></div>
    <img id="dynamicStoreMap" class="store-map-img" src="" style="display:none;">
    <div class="btn-box" id="guideNavBox">
      <button class="btn btn-p" onclick="nav('p-cart')">Enter Billing Mode</button>
    </div>
    <div class="footer-mark">@instabasket.in</div>
  </div>

  <div id="p-cart" class="container">
    <header>
      <div class="logo">My Basket</div>
      <button class="btn-outline" onclick="showMapFromCart()">Store Map</button>
    </header>
    <div class="ad-placeholder"></div>
    <div class="stat-bar">
      <div><p>ITEMS</p><h2 id="c-count">0</h2></div>
      <div><p>TOTAL</p><h2 id="c-total">₹0.00</h2></div>
    </div>
    <div id="c-list" style="padding-bottom: 20px;"></div>
    <div class="btn-box">
      <button class="btn btn-p" style="background:var(--success);" onclick="checkout()">Checkout Now</button>
      <button class="btn btn-s" style="color:var(--danger);" onclick="resetSession()">Discard Session</button>
    </div>
    <div class="footer-mark">@instabasket.in</div>
  </div>

  <div id="p-checkout" class="container">
    <header><div class="logo">Receipt</div></header>
    <div class="ad-placeholder"></div>
    <div class="qr-box" style="text-align:center; padding: 20px;">
      <h3 style="color:var(--text-s);">Total Bill Amount</h3>
      <h1 id="bill-amount" style="font-size:48px; margin: 10px 0; color:var(--success);">₹0.00</h1>
      
      <div id="qr-container"></div>
      
      <p style="font-size:12px; color:#555; margin-top: 10px;">Customer ID: <span id="cust-id-display"></span></p>
      
      <div class="btn-box">
          <button class="btn btn-s" style="background:#4b5563; color:white;" onclick="generateCashQR()">Pay by Cash</button>
          <button class="btn-test" id="test-pay-btn" onclick="generateInvoiceQR()">Payment Done. (for testing purpose only)</button>
          <button class="btn btn-p" onclick="triggerHardwareReset()">Complete & Exit</button>
      </div>
    </div>
    <div class="footer-mark">@instabasket.in</div>
  </div>

  <div id="p-admin-login" class="container">
    <header>
      <button style="background:none; border:none; color:white; font-size:24px;" onclick="nav('p-land')">←</button>
      <div class="logo">Admin Portal</div>
    </header>
    <div style="padding: 20px;">
      <div class="input-group"><label>Mart Code</label><input type="text" id="martCode" placeholder="Enter Mart ID"></div>
      <div class="input-group"><label>Password</label><input type="password" id="martPass" placeholder="••••••••"></div>
      <div class="btn-box">
        <button class="btn btn-p" onclick="validateAdmin()">Login Portal</button>
        <p style="text-align: center; color: var(--text-s); font-size: 14px; margin-top: 10px;">New Partner?</p>
        <button class="btn btn-outline" onclick="openCreateStore()">Create New Store</button>
      </div>
    </div>
  </div>

  <div id="p-new-store" class="container">
    <header><button style="background:none; border:none; color:white; font-size:24px;" onclick="nav('p-admin-login')">←</button><div class="logo">Partner</div></header>
    <div id="generatedID" style="text-align:center; padding:40px; font-size:32px; color:var(--accent); font-weight:bold;">MART-XXXX</div>
    <div class="input-group"><label>Set Password</label><input type="password" id="newStorePass" placeholder="Set Password"></div>
    <div class="btn-box"><button class="btn btn-p" onclick="saveNewStore()">Initialize Store</button></div>
  </div>

  <div id="p-ob-floors" class="container">
    <header><button style="background:none; border:none; color:white; font-size:24px;" onclick="nav('p-admin-login')">←</button><div class="logo">Setup</div></header>
    <div style="padding: 20px;">
      <div class="input-group"><label>Total Floors</label><input type="number" id="floorCount" placeholder="e.g. 3"></div>
      <div class="input-group"><label>Map Image URL</label><input type="text" id="mapImageUrl" placeholder="https://image-link.com"></div>
      <div class="btn-box"><button class="btn btn-p" onclick="setupFloors()">Next Step</button></div>
    </div>
  </div>

  <div id="p-ob-mapping" class="container">
    <header><button style="background:none; border:none; color:white; font-size:24px;" onclick="nav('p-ob-floors')">←</button><div class="logo">Layout</div></header>
    <div id="floorMappingArea" style="padding:20px;"></div>
    <div class="btn-box"><button class="btn btn-p" onclick="saveFloorMapping()">Finish Setup</button></div>
  </div>

  <script type="module">
    import { initializeApp } from "https://www.gstatic.com/firebasejs/10.7.1/firebase-app.js";
    import { getFirestore, doc, setDoc, getDoc, updateDoc, deleteDoc } from "https://www.gstatic.com/firebasejs/10.7.1/firebase-firestore.js";

    const firebaseConfig = {
      apiKey: "AIzaSyCCnzxKihi5R3EBXlQoRABLoH0Z-OGl2R4",
      authDomain: "smartcart-d7f33.firebaseapp.com",
      projectId: "smartcart-d7f33",
      storageBucket: "smartcart-d7f33.firebasestorage.app",
      messagingSenderId: "501090201683",
      appId: "1:501090201683:web:d6baa4f01bd47dc0af16a6"
    };

    const app = initializeApp(firebaseConfig);
    const db = getFirestore(app);
    let currentMartId = "";
    let customerStoreData = null;
    let existingFloorData = {}; 
    let poll;
    let localCartData = { items: [], total: 0, itemCount: 0 };
    let currentCustID = ""; 
    let currentSlide = 0;

    const SECTIONS = ["Groceries", "Fruits & Vegetables", "Toys & Games", "Households", "Electronics", "Apparel", "Beauty & Care", "Beverages", "Dairy & Frozen", "Personal Care"];
    const MARKET_DATABASE = {
      "Apparel": ["t-shirt", "jeans", "shirt", "jacket", "socks"],
      "Groceries": ["maggie", "oats", "sugar", "rice", "salt", "apple", "spinach"],
      "Dairy & Frozen": ["milk", "butter", "cheese", "yogurt", "ice cream"],
      "Electronics": ["headphones", "mouse", "keyboard", "laptop", "usb hub"],
      "Personal Care": ["soap", "shampoo", "toothpaste", "perfume", "lotion"]
    };

    function startSliders() {
      setInterval(() => {
        currentSlide = (currentSlide + 1) % 4;
        document.querySelectorAll('.slider').forEach(s => {
          s.style.transform = `translateX(-${currentSlide * 25}%)`;
        });
      }, 4000);
    }

    window.nav = function(id) {
      document.querySelectorAll('.container').forEach(c => c.classList.remove('active'));
      const activePage = document.getElementById(id);
      activePage.classList.add('active');
      const placeholder = activePage.querySelector('.ad-placeholder');
      if(placeholder && !placeholder.innerHTML) {
        const tmpl = document.getElementById('ad-slider-tmpl').content.cloneNode(true);
        placeholder.appendChild(tmpl);
      }
      if(id === 'p-cart') { poll = setInterval(upd, 800); } else { clearInterval(poll); }
    };

    // --- ADMIN LOGIC ---
    window.openCreateStore = () => {
      const randomID = "MART-" + Math.floor(1000 + Math.random() * 9000);
      document.getElementById('generatedID').innerText = randomID;
      nav('p-new-store');
    };

    window.saveNewStore = async () => {
      const id = document.getElementById('generatedID').innerText;
      const pass = document.getElementById('newStorePass').value;
      if(!pass) return alert("Enter password");
      try {
        await setDoc(doc(db, "marts", id), { password: pass, configured: false });
        alert("Store Registered! Login to configure layout.");
        nav('p-admin-login');
      } catch (e) { alert("Error: " + e.message); }
    };

    window.validateAdmin = async () => {
      const code = document.getElementById('martCode').value;
      const pass = document.getElementById('martPass').value;
      try {
        const docRef = doc(db, "marts", code);
        const docSnap = await getDoc(docRef);
        if (docSnap.exists() && docSnap.data().password === pass) {
          currentMartId = code;
          const data = docSnap.data();
          if(data.configured) {
            document.getElementById('floorCount').value = data.totalFloors;
            document.getElementById('mapImageUrl').value = data.mapImageUrl || "";
            existingFloorData = data.floorDetails || {}; 
            alert("Welcome back! Loading previous configuration...");
          } else {
            existingFloorData = {}; 
          }
          nav('p-ob-floors');
        } else alert("Invalid credentials");
      } catch (e) { alert("Login failed"); }
    };

    window.setupFloors = () => {
      const count = parseInt(document.getElementById('floorCount').value);
      if(!count || count < 1) return alert("Enter valid floor count");
      
      const area = document.getElementById('floorMappingArea');
      area.innerHTML = "<h3>Assign Sections</h3>";
      for(let i=1; i<=count; i++) {
        const previousSelections = existingFloorData[`floor_${i}`] || [];
        let options = SECTIONS.map(s => {
          const isSelected = previousSelections.includes(s) ? 'selected' : '';
          return `<option value="${s}" ${isSelected}>${s}</option>`;
        }).join('');

        area.innerHTML += `
          <div class="floor-box">
            <label>Floor ${i} Sections</label>
            <select id="fsel-${i}" multiple style="height:100px; margin-top:5px;">${options}</select>
            <small style="color:var(--accent)">Hold Ctrl/Cmd to select multiple</small>
          </div>`;
      }
      nav('p-ob-mapping');
    };

    window.saveFloorMapping = async () => {
      const count = document.getElementById('floorCount').value;
      const mapImg = document.getElementById('mapImageUrl').value;
      const floorData = {};
      for(let i=1; i<=count; i++) {
        const selected = Array.from(document.getElementById(`fsel-${i}`).selectedOptions).map(o => o.value);
        floorData[`floor_${i}`] = selected;
      }
      try {
        await updateDoc(doc(db, "marts", currentMartId), { 
          totalFloors: count,
          mapImageUrl: mapImg,
          floorDetails: floorData,
          configured: true 
        });
        alert("Store Layout Updated!");
        nav('p-land');
      } catch (e) { alert("Save failed: " + e.message); }
    };

    // --- CUSTOMER & CART LOGIC ---
    window.customerLogin = async () => {
      const mid = document.getElementById('targetMartId').value;
      if(!mid) return alert("Enter Mart ID");
      try {
        const docSnap = await getDoc(doc(db, "marts", mid));
        if (docSnap.exists() && docSnap.data().configured) {
          customerStoreData = docSnap.data();
          currentMartId = mid;
          nav('p-shopping-list');
        } else alert("Mart not found or not configured!");
      } catch (e) { alert("Error"); }
    };

    window.processList = () => {
      const input = document.getElementById('userListInput').value.toLowerCase();
      const searchItems = input.split(',').map(i => i.trim()).filter(i => i !== "");
      if(searchItems.length === 0) return alert("Empty list");

      let html = "<h3>Item Locations:</h3>";
      searchItems.forEach(query => {
        let foundCategory = "General";
        let foundFloor = "Unknown";
        for (let category in MARKET_DATABASE) {
          if (MARKET_DATABASE[category].includes(query)) { foundCategory = category; break; }
        }
        if (customerStoreData && customerStoreData.floorDetails) {
          for (let floorName in customerStoreData.floorDetails) {
            if (customerStoreData.floorDetails[floorName].includes(foundCategory)) {
              foundFloor = floorName.replace("_", " ");
              break;
            }
          }
        }
        html += `<div class="card"><div><b>${query.toUpperCase()}</b><br><small>Category: ${foundCategory}</small></div><span class="loc-tag">${foundFloor}</span></div>`;
      });
      showFloorPlan(false, html);
    };

    window.showFloorPlan = (onlyPlan = false, prefixHtml = "") => {
      let area = document.getElementById('guideArea');
      let imgMap = document.getElementById('dynamicStoreMap');
      if(customerStoreData && customerStoreData.mapImageUrl) {
          imgMap.src = customerStoreData.mapImageUrl;
          imgMap.style.display = 'block';
      } else { imgMap.style.display = 'none'; }

      let html = prefixHtml || "<h3>Store Directory:</h3>";
      if(customerStoreData && customerStoreData.floorDetails) {
          Object.keys(customerStoreData.floorDetails).forEach(f => {
            html += `<div class="card"><b>${f.replace("_", " ")}</b><small>${customerStoreData.floorDetails[f].join(", ")}</small></div>`;
          });
      }
      area.innerHTML = html;
      nav('p-store-guide');
    };

    window.showMapFromCart = () => {
      window.showFloorPlan(true); 
      document.getElementById('closeMapBtn').style.display = 'block';
    };

    window.checkout = async () => {
      if(localCartData.itemCount === 0) return alert("Empty!");
      currentCustID = "INSTA-" + Math.floor(10000 + Math.random() * 90000);
      try {
        await setDoc(doc(db, "marts", currentMartId, "sales", currentCustID), {
          customerID: currentCustID, items: localCartData.items, total: localCartData.total, status: "pending"
        });
        document.getElementById('bill-amount').innerText = "₹" + localCartData.total.toFixed(2);
        document.getElementById('cust-id-display').innerText = currentCustID;
        document.getElementById('qr-container').innerHTML = ""; 
        nav('p-checkout');
      } catch (e) { alert("Error creating checkout"); }
    };

    window.generateInvoiceQR = async () => {
      try {
        const saleRef = doc(db, "marts", currentMartId, "sales", currentCustID);
        await updateDoc(saleRef, {
          status: "successful",
          method: "online"
        });

        const baseUrl = "https://smartcart-d7f33.web.app/index.html";
        const fullLink = `${baseUrl}?id=${currentCustID}&mart=${currentMartId}`;
        const qrUrl = `https://quickchart.io/qr?text=${encodeURIComponent(fullLink)}&size=250`;      
        document.getElementById('qr-container').innerHTML = `
          <p style="color:var(--success); font-size:14px; margin-bottom:10px;">Payment Verified! Scan for Receipt:</p>
          <img src="${qrUrl}" class="qr-img">
        `;
        alert("Payment Simulated Successfully! Status updated to Successful.");
      } catch (e) {
        alert("Error updating payment status.");
      }
    };

    window.generateCashQR = async () => {
      try {
        const saleRef = doc(db, "marts", currentMartId, "sales", currentCustID);
        await updateDoc(saleRef, {
          status: "pending_cash",
          method: "cash"
        });

        const cashBaseUrl = "https://smartcart-d7f33.web.app/cashpayreceipt.html";
        const cashFullLink = `${cashBaseUrl}?id=${currentCustID}&mart=${currentMartId}`;
        const qrUrl = `https://quickchart.io/qr?text=${encodeURIComponent(cashFullLink)}&size=250`;
        
        document.getElementById('qr-container').innerHTML = `
          <p style="color:var(--accent); font-size:14px; margin-bottom:10px;">Please pay at counter. Scan for Cash Receipt:</p>
          <img src="${qrUrl}" class="qr-img">
        `;
        alert("Cash payment initiated. Please visit the counter.");
      } catch (e) {
        alert("Error generating cash receipt QR.");
      }
    };

    const upd = async () => {
      try {
        const response = await fetch('/api/cart');
        const data = await response.json();
        localCartData = data;
        document.getElementById('c-count').innerText = data.itemCount;
        document.getElementById('c-total').innerText = `₹${data.total.toFixed(2)}`;
        document.getElementById('c-list').innerHTML = data.items.map(i => `<div class="card"><b>${i.name}</b><span>x${i.qty}</span></div>`).join('');
      } catch (e) {}
    };

    window.triggerHardwareReset = () => { fetch('/api/restart'); setTimeout(() => location.reload(), 500); };
    window.resetSession = () => { if(confirm("Discard all items?")) window.triggerHardwareReset(); };

    startSliders();
  </script>
</body>
</html>
)rawliteral";

void handleRoot() { server.send(200, "text/html", index_html); }

void handleCart() {
  String json = "{\"items\":[";
  float total = 0; int qty = 0;
  for (int i = 0; i < cartCount; i++) {
    if (i > 0) json += ",";
    json += "{\"name\":\"" + cart[i].name + "\",\"price\":" + String(cart[i].price) + ",\"qty\":" + String(cart[i].quantity) + "}";
    total += cart[i].price * cart[i].quantity;
    qty += cart[i].quantity;
  }
  json += "],\"total\":" + String(total) + ",\"itemCount\":" + String(qty) + "}";
  server.send(200, "application/json", json);
}

void handleRestart() {
  server.send(200, "text/plain", "Restarting...");
  delay(1000);
  ESP.restart();
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); }
  server.on("/", handleRoot);
  server.on("/api/cart", handleCart);
  server.on("/api/restart", handleRestart);
  server.begin();
}

void loop() {
  server.handleClient();
  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
  byte currentUID[4];
  for (byte i = 0; i < 4; i++) currentUID[i] = rfid.uid.uidByte[i];
  if (!compareUID(currentUID, lastCardUID)) {
    copyUID(lastCardUID, currentUID);
    cardDetectedTime = millis();
    cardHeld = false;
    int idx = getItemIndexFromUID(currentUID);
    addToCart(currentUID, items[idx].name, items[idx].price);
  } else if (!cardHeld && (millis() - cardDetectedTime >= HOLD_TIME)) {
    cardHeld = true;
    removeFromCart(currentUID);
  }
  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
