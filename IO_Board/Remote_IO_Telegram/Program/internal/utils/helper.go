package utils

import "strings"

func GetParentTopic(topic string) string {
	parts := strings.Split(topic, "/")
	if len(parts) > 0 {
		return parts[0] // The parent topic is the first part
	}
	return "" // Return empty if the topic doesn't contain "/"
}

func GetChildTopic(topic string) string {
	parts := strings.Split(topic, "/")
	if len(parts) > 0 {
		return parts[1] // The parent topic is the first part
	}
	return "" // Return empty if the topic doesn't contain "/"
}

func AddSuffix(StringList []string, suffix string) []string {
    newList := make([]string, len(StringList)) // Create a new slice of the same length
    for i, name := range StringList {
        newList[i] = name + suffix
    }
    return newList
}

func FindIndex(StringList []string, value string) int {
    for i, name := range StringList {
        if name == value {
            return i
        }
    }
    return -1 // Return -1 if the value is not found
}
