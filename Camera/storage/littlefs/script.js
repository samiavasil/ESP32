
document.addEventListener("DOMContentLoaded", () => {
  // Check if user is logged in
  fetch("/check-session", { method: "GET", credentials: "same-origin" })
    .then((response) => response.json())
    .then((data) => {
      if (!data.loggedIn) {
        window.location.href = "login.html";  // Пренасочване към login.html, ако не е логнат
      }
    })
    .catch((error) => {
      console.error("Error checking session:", error);
      window.location.href = "login.html";  // Пренасочване при грешка
    });
  
  // WebSocket connection
  const socket = new WebSocket("ws://" + window.location.hostname + ":" +window.location.port + "/ws")

  socket.onopen = (e) => {
    console.log("WebSocket connection established")
    // Initial data request
    sendMessage({ type: "requestData", tab: "overview" })
  }

  socket.onmessage = (event) => {
    const data = JSON.parse(event.data)
    updateDashboard(data)
  }

  socket.onclose = (event) => {
    if (event.wasClean) {
      console.log(`WebSocket connection closed cleanly, code=${event.code} reason=${event.reason}`)
    } else {
      console.log("WebSocket connection died")
    }
  }

  socket.onerror = (error) => {
    console.log(`WebSocket error: ${error.message}`)
  }

  function sendMessage(message) {
    socket.send(JSON.stringify(message))
  }

  // Update dashboard with received data
  function updateDashboard(data) {
    if (data.totalUsers) updateTotalUsers(data.totalUsers)
    if (data.accessEvents) updateAccessEvents(data.accessEvents)
    if (data.systemStatus) updateSystemStatus(data.systemStatus)
    if (data.recentActivity) updateRecentActivity(data.recentActivity)
    if (data.systemResources) updateSystemResources(data.systemResources)
    if (data.users) updateUsers(data.users)
    if (data.accessLogs) updateAccessLogs(data.accessLogs)
  }

  // Update total users
  function updateTotalUsers(totalUsers) {
    const totalUsersElement = document.querySelector("#overview .card:nth-child(1) .stat-value")
    const totalUsersChange = document.querySelector("#overview .card:nth-child(1) .stat-desc")
    if (totalUsersElement) totalUsersElement.textContent = totalUsers.count
    if (totalUsersChange) totalUsersChange.textContent = totalUsers.change
  }

  // Update access events
  function updateAccessEvents(accessEvents) {
    const accessEventsElement = document.querySelector("#overview .card:nth-child(2) .stat-value")
    const accessEventsDesc = document.querySelector("#overview .card:nth-child(2) .stat-desc")
    if (accessEventsElement) accessEventsElement.textContent = accessEvents.count
    if (accessEventsDesc)
      accessEventsDesc.textContent = `${accessEvents.granted} granted, ${accessEvents.denied} denied`
  }

  // Update system status
  function updateSystemStatus(systemStatus) {
    const statusDot = document.querySelector("#overview .card:nth-child(3) .status-dot")
    const statusText = document.querySelector("#overview .card:nth-child(3) .status-indicator span")
    const lastRestart = document.querySelector("#overview .card:nth-child(3) .stat-desc")
    if (statusDot) statusDot.className = `status-dot ${systemStatus.status}`
    if (statusText) statusText.textContent = systemStatus.status
    if (lastRestart) lastRestart.textContent = `Last restart: ${systemStatus.lastRestart}`
  }

  // Update recent activity
  function updateRecentActivity(recentActivity) {
    const activityTable = document.querySelector("#overview .data-table tbody")
    if (activityTable) {
      activityTable.innerHTML = recentActivity
        .map(
          (activity) => `
        <tr>
          <td>${activity.time}</td>
          <td>${activity.user}</td>
          <td>${activity.cardId}</td>
          <td>${activity.accessPoint}</td>
          <td><span class="badge ${activity.status.toLowerCase()}">${activity.status}</span></td>
        </tr>
      `,
        )
        .join("")
    }
  }

  // Update system resources
  function updateSystemResources(systemResources) {
    const resources = ["memory", "cpu", "storage"]
    resources.forEach((resource) => {
      const fill = document.querySelector(`.progress-item:nth-child(${resources.indexOf(resource) + 1}) .progress-fill`)
      const value = document.querySelector(
        `.progress-item:nth-child(${resources.indexOf(resource) + 1}) .progress-header span:last-child`,
      )
      if (fill) fill.style.width = `${systemResources[resource]}%`
      if (value) value.textContent = `${systemResources[resource]}%`
    })
  }

  // Update users table
  function updateUsers(users) {
    const usersTable = document.querySelector("#users .data-table tbody")
    if (usersTable) {
      usersTable.innerHTML = users
        .map(
          (user) => `
        <tr>
          <td>${user.name}</td>
          <td>${user.cardId}</td>
          <td>${user.accessLevel}</td>
          <td><span class="badge ${user.status.toLowerCase()}">${user.status}</span></td>
          <td>
            <div class="action-buttons">
              <button class="btn btn-sm btn-outline" onclick="editUser('${user.id}')">Edit</button>
              <button class="btn btn-sm btn-danger" onclick="deleteUser('${user.id}')">Delete</button>
            </div>
          </td>
        </tr>
      `,
        )
        .join("")
    }
  }

  // Update access logs
  function updateAccessLogs(logs) {
    const logsTable = document.querySelector("#logs .data-table tbody")
    if (logsTable) {
      logsTable.innerHTML = logs
        .map(
          (log) => `
        <tr>
          <td>${log.date}</td>
          <td>${log.time}</td>
          <td>${log.user}</td>
          <td>${log.cardId}</td>
          <td>${log.accessPoint}</td>
          <td><span class="badge ${log.status.toLowerCase()}">${log.status}</span></td>
        </tr>
      `,
        )
        .join("")
    }
  }

  // Sidebar toggle
  const sidebar = document.getElementById("sidebar")
  const toggleBtn = document.getElementById("toggle-sidebar")

  toggleBtn.addEventListener("click", () => {
    sidebar.classList.toggle("collapsed")
  })

  // Tab navigation
  const tabButtons = document.querySelectorAll(".tab-btn")
  const tabPanes = document.querySelectorAll(".tab-pane")
  const pageTitle = document.getElementById("page-title")

  tabButtons.forEach((button) => {
    button.addEventListener("click", function () {
      const tabId = this.dataset.tab

      // Update active tab button
      tabButtons.forEach((btn) => btn.classList.remove("active"))
      this.classList.add("active")

      // Update active tab pane
      tabPanes.forEach((pane) => pane.classList.remove("active"))
      document.getElementById(tabId).classList.add("active")

      // Update page title
      pageTitle.textContent = this.textContent

      // Request data for the active tab
      sendMessage({ type: "requestData", tab: tabId })
    })
  })

  // Sidebar navigation
  const navLinks = document.querySelectorAll(".nav-link")

  navLinks.forEach((link) => {
    link.addEventListener("click", function (e) {
      e.preventDefault()

      const tabId = this.dataset.tab

      // Update active nav link
      navLinks.forEach((navLink) => navLink.classList.remove("active"))
      this.classList.add("active")

      // Update active tab button
      tabButtons.forEach((btn) => {
        if (btn.dataset.tab === tabId) {
          btn.click()
        }
      })
    })
  })

  // Range input value display
  const relayTimeInput = document.getElementById("relay-time")
  const relayTimeValue = document.getElementById("relay-time-value")

  if (relayTimeInput && relayTimeValue) {
    relayTimeInput.addEventListener("input", function () {
      relayTimeValue.textContent = this.value
      sendMessage({ type: "updateSetting", setting: "relayTime", value: this.value })
    })
  }

  // IP Mode toggle
  const ipModeSelect = document.getElementById("ip-mode")
  const ipFields = document.querySelectorAll("#ip-address, #subnet, #gateway, #dns")

  if (ipModeSelect) {
    ipModeSelect.addEventListener("change", function () {
      const isStatic = this.value === "Static IP"

      ipFields.forEach((field) => {
        field.disabled = !isStatic
      })

      sendMessage({ type: "updateSetting", setting: "ipMode", value: this.value })
    })
  }

  // Add event listeners for settings changes
  const settingsInputs = document.querySelectorAll("#settings input, #settings select")
  settingsInputs.forEach((input) => {
    input.addEventListener("change", function () {
      sendMessage({ type: "updateSetting", setting: this.id, value: this.value })
    })
  })

  // Functions for user management
  window.editUser = (userId) => {
    console.log("Edit user:", userId)
    // Implement edit user functionality
    sendMessage({ type: "editUser", userId: userId })
  }

  window.deleteUser = (userId) => {
    if (confirm("Are you sure you want to delete this user?")) {
      console.log("Delete user:", userId)
      sendMessage({ type: "deleteUser", userId: userId })
    }
  }

  // Function to add a new user
  document.querySelector("#users .btn-primary").addEventListener("click", () => {
    // Implement add user functionality
    console.log("Add new user")
    sendMessage({ type: "addUser" })
  })


  // Simulate data updates (for demo purposes)
  function updateSystemResources() {
    const memoryUsage = Math.floor(Math.random() * 30) + 50 // 50-80%
    const cpuUsage = Math.floor(Math.random() * 40) + 20 // 20-60%
    const storageUsage = Math.floor(Math.random() * 30) + 10 // 10-40%

    const memoryFill = document.querySelector(".progress-item:nth-child(1) .progress-fill")
    const memoryValue = document.querySelector(".progress-item:nth-child(1) .progress-header span:last-child")

    const cpuFill = document.querySelector(".progress-item:nth-child(2) .progress-fill")
    const cpuValue = document.querySelector(".progress-item:nth-child(2) .progress-header span:last-child")

    const storageFill = document.querySelector(".progress-item:nth-child(3) .progress-fill")
    const storageValue = document.querySelector(".progress-item:nth-child(3) .progress-header span:last-child")

    if (memoryFill && memoryValue) {
      memoryFill.style.width = memoryUsage + "%"
      memoryValue.textContent = memoryUsage + "%"
    }

    if (cpuFill && cpuValue) {
      cpuFill.style.width = cpuUsage + "%"
      cpuValue.textContent = cpuUsage + "%"
    }

    if (storageFill && storageValue) {
      storageFill.style.width = storageUsage + "%"
      storageValue.textContent = storageUsage + "%"
    }
  }

  // Update system resources every 5 seconds
  setInterval(updateSystemResources, 5000)

  // Initial update
  updateSystemResources()

  // Add logout functionality
  const logoutBtn = document.querySelector(".logout-btn")
  logoutBtn.addEventListener("click", () => {
    sessionStorage.removeItem("loggedIn")
    window.location.href = "login.html"
  })
})

