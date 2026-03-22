package connector

import (
	"errors"
	"io"
	"net/http"

	"github.com/gin-gonic/gin"
)

func (tc *TsConnector) TcHostedUpload(ctx *gin.Context) {
	username, ok := tc.extractUserContext(ctx)
	if !ok {
		return
	}

	ctx.Request.Body = http.MaxBytesReader(ctx.Writer, ctx.Request.Body, 2<<30)

	file, header, err := ctx.Request.FormFile("file")
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "file is required: " + err.Error(), "ok": false})
		return
	}
	defer file.Close()

	content, err := io.ReadAll(file)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": "failed to read file: " + err.Error(), "ok": false})
		return
	}

	slug := ctx.PostForm("slug")
	mimeType := ctx.PostForm("mime_type")
	fileName := header.Filename

	data, err := tc.teamserver.TsHostedUpload(username, slug, fileName, mimeType, content)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{
		"ok":      true,
		"message": "File hosted",
		"file_id": data.FileId,
		"slug":    data.Slug,
	})
}

func (tc *TsConnector) TcHostedList(ctx *gin.Context) {
	jsonFiles, err := tc.teamserver.TsHostedList()
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}
	ctx.Data(http.StatusOK, "application/json; charset=utf-8", []byte(jsonFiles))
}

type HostedDeleteRequest struct {
	FileIdArray []string `json:"file_id_array"`
}

func (tc *TsConnector) TcHostedDelete(ctx *gin.Context) {
	var req HostedDeleteRequest
	err := ctx.ShouldBindJSON(&req)
	if err != nil {
		_ = ctx.Error(errors.New("invalid request"))
		return
	}

	err = tc.teamserver.TsHostedDelete(req.FileIdArray)
	if err != nil {
		ctx.JSON(http.StatusOK, gin.H{"message": err.Error(), "ok": false})
		return
	}

	ctx.JSON(http.StatusOK, gin.H{"message": "Files deleted", "ok": true})
}

