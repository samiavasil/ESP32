document.addEventListener("DOMContentLoaded", () => {
  const loginForm = document.getElementById("login-form")
  const loginError = document.getElementById("login-error")

  loginForm.addEventListener("submit", (e) => {
    e.preventDefault()

    const username = document.getElementById("username").value
    const password = document.getElementById("password").value

    // Изпращане на POST заявка за логин към сървъра
    fetch("/login", {
      method: "POST",
      headers: {
        "Content-Type": "application/json",
      },
      body: JSON.stringify({ username: username, password: password }),
    })
    .then((response) => response.json())
    .then((data) => {
      if (data.success) {
        // Ако логинът е успешен, пренасочваме към index.html
      //  sessionStorage.setItem("auth", "true");
        window.location.href = "index.html"
      } else {
        // Ако логинът е неуспешен, показваме съобщение за грешка
        loginError.textContent = "Invalid username or password"
      }
    })
    .catch((error) => {
      console.error("Login error:", error)
      loginError.textContent = "Connection error. Please try again."
    })
  })
})