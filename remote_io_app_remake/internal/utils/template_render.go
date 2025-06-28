package utils

import (
	"html/template"
	"net/http"
	"log"
	"path/filepath"
)

// RenderTemplate renders an HTML template with data and supports layouts and inner content.
func RenderTemplate(w http.ResponseWriter, tmpl string, base string, data interface{}) error {
	// Define the template directory
	tmplPath := filepath.Join("src", tmpl)
	layoutPath := filepath.Join("src", base)

	// Parse the main template (layout) and any specific templates (e.g., home.html)
	tmplParsed, err := template.ParseFiles(layoutPath, tmplPath)
	if err != nil {
		log.Printf("Error parsing template: %v", err)
		return err
	}

	// Execute the template, passing in the data
	err = tmplParsed.ExecuteTemplate(w, base, data)
	if err != nil {
		log.Printf("Error executing template: %v", err)
		return err
	}

	return nil
}

